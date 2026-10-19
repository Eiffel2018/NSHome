#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Extract only a matching Album loader executable/metadata pair."""
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from verify import pfs, require


def unpack(package, destination):
    files = pfs(Path(package).read_bytes())
    nso, npdm = files['main'], files['main.npdm']
    require(nso[:4] == b'NSO0', 'loader NSO header')
    require(npdm[:4] == b'META', 'loader metadata header')
    aci = struct.unpack_from('<I', npdm, 0x70)[0]
    require(npdm[aci:aci + 4] == b'ACI0', 'loader ACI0')
    require(struct.unpack_from('<Q', npdm, aci + 0x10)[0] == 0x010000000000100D, 'loader must use Album ID')
    target = Path(destination)
    target.mkdir(parents=True, exist_ok=True)
    for name, data in files.items():
        (target / name).write_bytes(data)


if __name__ == '__main__':
    unpack(*sys.argv[1:])
