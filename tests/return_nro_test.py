#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from verify import verify_return_nro


def fixture():
    data = bytearray(0x80 + 0x38 + 0x4000)
    data[0x10:0x14] = b'NRO0'
    struct.pack_into('<I', data, 0x18, 0x80)
    struct.pack_into('<II', data, 0x20, 0, 0x80)
    data[0x80:0x84] = b'ASET'
    struct.pack_into('<QQ', data, 0x80 + 24, 0x38, 0x4000)
    data[0xB8:0xB8 + 14] = b'Return to HOME'
    return data


class ReturnNroTests(unittest.TestCase):
    def test_expected_tool(self):
        verify_return_nro(fixture())

    def test_truncation(self):
        for length in (0, 0x20, 0x80, 0xB8, len(fixture()) - 1):
            with self.subTest(length=length), self.assertRaises(ValueError):
                verify_return_nro(fixture()[:length])

    def test_wrong_tool(self):
        data = fixture()
        data[0xB8] = ord('X')
        with self.assertRaisesRegex(ValueError, 'title'):
            verify_return_nro(data)

    def test_bad_asset_and_segment_ranges(self):
        for offset in (0x18, 0x20, 0x80 + 24, 0x80 + 32):
            data = fixture()
            struct.pack_into('<I', data, offset, 0xFFFFFFFF)
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                verify_return_nro(data)


if __name__ == '__main__':
    unittest.main()
