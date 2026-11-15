"""Run an isolated process tree; never leave compiler descendants after timeout."""

import ctypes
import json
import os
import signal
import subprocess
import sys
import time


class WindowsJob:
    """A private job, assigned before a gated launcher can start the build."""

    def __init__(self):
        from ctypes import wintypes

        class BasicLimits(ctypes.Structure):
            _fields_ = [("process_time", ctypes.c_int64), ("job_time", ctypes.c_int64),
                        ("flags", wintypes.DWORD), ("min_working_set", ctypes.c_size_t),
                        ("max_working_set", ctypes.c_size_t), ("active_limit", wintypes.DWORD),
                        ("affinity", ctypes.c_size_t), ("priority", wintypes.DWORD),
                        ("scheduling", wintypes.DWORD)]

        class ExtendedLimits(ctypes.Structure):
            _fields_ = [("basic", BasicLimits), ("io", ctypes.c_uint64 * 6),
                        ("memory", ctypes.c_size_t * 4)]

        self.api = ctypes.WinDLL("kernel32", use_last_error=True)
        for name, result, arguments in (
            ("CreateJobObjectW", wintypes.HANDLE, [ctypes.c_void_p, wintypes.LPCWSTR]),
            ("SetInformationJobObject", wintypes.BOOL,
             [wintypes.HANDLE, ctypes.c_int, ctypes.c_void_p, wintypes.DWORD]),
            ("AssignProcessToJobObject", wintypes.BOOL, [wintypes.HANDLE, wintypes.HANDLE]),
            ("OpenProcess", wintypes.HANDLE, [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]),
            ("TerminateJobObject", wintypes.BOOL, [wintypes.HANDLE, wintypes.UINT]),
            ("QueryInformationJobObject", wintypes.BOOL,
             [wintypes.HANDLE, ctypes.c_int, ctypes.c_void_p, wintypes.DWORD, ctypes.c_void_p]),
            ("CloseHandle", wintypes.BOOL, [wintypes.HANDLE]),
        ):
            function = getattr(self.api, name)
            function.restype, function.argtypes = result, arguments
        self.handle = self.api.CreateJobObjectW(None, None)
        if not self.handle:
            raise ctypes.WinError(ctypes.get_last_error())
        limits = ExtendedLimits()
        limits.basic.flags = 0x2000  # JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
        if not self.api.SetInformationJobObject(self.handle, 9, ctypes.byref(limits), ctypes.sizeof(limits)):
            self.close()
            raise ctypes.WinError(ctypes.get_last_error())

    def assign(self, pid):
        process = self.api.OpenProcess(0x0101, False, pid)  # SET_QUOTA | TERMINATE
        if not process:
            raise ctypes.WinError(ctypes.get_last_error())
        try:
            if not self.api.AssignProcessToJobObject(self.handle, process):
                raise ctypes.WinError(ctypes.get_last_error())
        finally:
            self.api.CloseHandle(process)

    def terminate(self):
        if not self.api.TerminateJobObject(self.handle, 1):
            raise ctypes.WinError(ctypes.get_last_error())
        # BasicAccountingInformation has four int64 times and four DWORDs;
        # ActiveProcesses is DWORD index 10. Wait for every descendant.
        accounting = (ctypes.c_uint32 * 12)()
        deadline = time.monotonic() + 10
        while True:
            if not self.api.QueryInformationJobObject(self.handle, 1, ctypes.byref(accounting),
                                                      ctypes.sizeof(accounting), None):
                raise ctypes.WinError(ctypes.get_last_error())
            if accounting[10] == 0:
                return
            if time.monotonic() >= deadline:
                raise RuntimeError("Build job did not terminate")
            time.sleep(0.01)

    def close(self):
        if self.handle:
            self.api.CloseHandle(self.handle)
            self.handle = None


def run(command, *, cwd, capture_output=True, timeout=300):
    if not capture_output:
        raise ValueError("Process-tree runs require captured output")
    job = WindowsJob() if os.name == "nt" else None
    # The launcher cannot spawn a compiler before job assignment. A process
    # group alone on Windows does not provide descendant lifetime ownership.
    launcher = ("import json,subprocess,sys; "
                "token=sys.stdin.buffer.read(1); "
                "sys.exit(subprocess.call(json.loads(sys.argv[1]), stdin=subprocess.DEVNULL) "
                "if token == b'1' else 125)")
    process = None
    try:
        process = subprocess.Popen([sys.executable, "-c", launcher, json.dumps(command)], cwd=cwd,
                                   stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                   start_new_session=job is None)
        if job:
            job.assign(process.pid)
        process.stdin.write(b"1")
        process.stdin.flush()
        stdout, stderr = process.communicate(timeout=timeout)
        if job:
            job.terminate()
        else:
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
        return subprocess.CompletedProcess(command, process.returncode, stdout, stderr)
    except BaseException as error:
        if process:
            try:
                if job:
                    job.terminate()
                else:
                    try:
                        os.killpg(process.pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
                stdout, stderr = process.communicate(timeout=10)
                if isinstance(error, subprocess.TimeoutExpired):
                    error.stdout = stdout or error.stdout or b""
                    error.stderr = stderr or error.stderr or b""
            except BaseException as cleanup_error:
                error.process_tree_cleanup_failed = True
                print(f"Process tree cleanup failed: {cleanup_error}", file=sys.stderr)
        raise
    finally:
        if job:
            job.close()
