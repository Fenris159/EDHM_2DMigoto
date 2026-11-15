"""A controlled timeout must stop the real launcher and its descendant."""

import ctypes
import os
import signal
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

import process_tree


class ProcessTreeTests(unittest.TestCase):
    def test_timeout_stops_descendant_before_returning_original_error(self):
        child = "import os,threading; print(os.getpid(), flush=True); threading.Event().wait()"
        parent = ("import subprocess,sys,threading; "
                  f"subprocess.Popen([sys.executable, '-c', {child!r}]); threading.Event().wait()")
        command = [sys.executable, "-c", parent]
        actual_popen = subprocess.Popen
        descendants = []
        timeout_error = subprocess.TimeoutExpired(command, 300)
        launched = []
        captured = []
        process_handle = None
        api = None
        if os.name == "nt":
            from ctypes import wintypes
            api = ctypes.WinDLL("kernel32", use_last_error=True)
            api.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
            api.OpenProcess.restype = wintypes.HANDLE
            api.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
            api.WaitForSingleObject.restype = wintypes.DWORD
            api.CloseHandle.argtypes = [wintypes.HANDLE]

        def launch(*args, **kwargs):
            process = actual_popen(*args, **kwargs)
            launched.append(process)
            communicate = process.communicate
            first = True

            def controlled_timeout(*arguments, **options):
                nonlocal first, process_handle
                if first:
                    first = False
                    # Readiness handshake replaces any real timeout or sleep.
                    ready = process.stdout.readline()
                    captured.append(ready)
                    descendants.append(int(ready))
                    if api:
                        process_handle = api.OpenProcess(0x00100000, False, descendants[0])
                        self.assertIsNotNone(process_handle)
                    timeout_error.stdout = ready
                    raise timeout_error
                return communicate(*arguments, **options)

            process.communicate = controlled_timeout
            return process

        try:
            with tempfile.TemporaryDirectory() as directory, patch.object(process_tree.subprocess, "Popen", launch):
                with self.assertRaises(subprocess.TimeoutExpired) as raised:
                    process_tree.run(command, cwd=directory)
            self.assertIs(raised.exception, timeout_error)
            self.assertEqual(timeout_error.stdout, captured[0])
            if api:
                self.assertEqual(api.WaitForSingleObject(process_handle, 0), 0, "descendant survived timeout cleanup")
            else:
                status = Path(f"/proc/{descendants[0]}/stat")
                try:
                    state = status.read_text().split()[2]
                except (FileNotFoundError, ProcessLookupError):
                    state = "Z"  # A reaped process has terminated too.
                self.assertEqual(state, "Z", "descendant survived timeout cleanup")
            self.assertIsNotNone(launched[0].returncode)
        finally:
            if api and process_handle:
                api.CloseHandle(process_handle)
            if not api and launched:
                try:
                    os.killpg(launched[0].pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass


if __name__ == "__main__":
    unittest.main()
