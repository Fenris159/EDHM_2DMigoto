"""Host filesystem lookup must accept Wine's lowercase WoW64 directory."""

from pathlib import Path
import struct
import tempfile
import unittest

from run_proxy_chain_tests import matching_version_library


class ProxyLibraryTests(unittest.TestCase):
    def test_selects_matching_architecture_from_lowercase_wow64(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for folder, machine in (("system32", 0x8664), ("syswow64", 0x014C)):
                target = root / folder / "version.dll"
                target.parent.mkdir()
                image = bytearray(128)
                image[:2] = b"MZ"
                struct.pack_into("<I", image, 60, 64)
                image[64:68] = b"PE\0\0"
                struct.pack_into("<H", image, 68, machine)
                target.write_bytes(image)
            self.assertEqual(matching_version_library(root, 0x014C), root / "syswow64" / "version.dll")
            self.assertEqual(matching_version_library(root, 0x8664), root / "system32" / "version.dll")


if __name__ == "__main__":
    unittest.main()
