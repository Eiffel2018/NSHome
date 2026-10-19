#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
import hashlib
import json
import sys
import zipfile
from pathlib import Path
from verify import verify, verify_return_nro

root = Path(__file__).resolve().parent
build = root / 'build'
verify(build / 'am-observer.elf', build / 'exefs.nsp', build / 'disassembly.txt')
if not json.loads((build / 'verification.json').read_text())['launch_bridge_connected']:
    raise ValueError('Rebuild the module with the game launch bridge before packaging')
if not json.loads((build / 'verification.json').read_text()).get('nro_launch_connected'):
    raise ValueError('Rebuild the matched module and loader with NRO launch support')
destination = Path(sys.argv[1]).resolve()
if destination.exists():
    raise FileExistsError(destination)
folder = 'atmosphere/contents/00FF4E5348414D01/'
return_nro = (root / 'tools/return_to_home/return-to-home.nro').read_bytes()
verify_return_nro(return_nro)
files = {
    'switch/ReturnToHOME.nro': return_nro,
    folder + 'exefs.nsp': (build / 'exefs.nsp').read_bytes(),
    folder + 'mitm.lst': b'appletAE',
    folder + 'flags/boot2.flag': b'',
    folder + 'breeze_loader/main': (build / 'breeze_loader/main').read_bytes(),
    folder + 'breeze_loader/main.npdm': (build / 'breeze_loader/main.npdm').read_bytes(),
    folder + 'breeze_loader/LICENSE.md': (root / 'loader/LICENSE.md').read_bytes(),
    'README.md': (root / 'README.md').read_bytes(),
    'INSTALL.md': (root / 'docs/INSTALL.md').read_bytes(),
    'docs/DEVELOPMENT.md': (root / 'docs/DEVELOPMENT.md').read_bytes(),
    'NOTICE.md': (root / 'NOTICE.md').read_bytes(),
    'LICENSE': (root / 'LICENSE').read_bytes(),
    'verification.json': (build / 'verification.json').read_bytes(),
    'extras/auto-unlock/config/NSHome/boot-touch-once.flag': b'',
    'extras/auto-unlock/config/NSHome/skip-lock-screen.flag': b'',
}
# Include the runnable PC samples alongside their shared client and reference.
for pattern in ('samples/*.py', 'samples/*.md', 'tools/nshome_remote.py', 'docs/REMOTE_6802.md', 'docs/LAUNCH.md'):
    for path in root.glob(pattern):
        files[path.relative_to(root).as_posix()] = path.read_bytes()
# Include corresponding module source and the pinned dependency build recipe.
for pattern in ('source/*', 'res/*', 'tests/*.cpp', 'tests/*.sh', 'tests/*.py', 'tests/**/*.h', 'tests/**/*.hpp', 'docs/*.md', 'tools/*.py', 'tools/return_to_home/main.c', 'tools/return_to_home/Makefile', 'tools/return_to_home/README.md', 'loader/Makefile', 'loader/hbl.json', 'loader/LICENSE.md', 'loader/source/*.c', 'loader/source/*.s', 'samples/*.py', 'samples/*.md'):
    for path in root.glob(pattern):
        if path.is_file():
            files['source/NSHome/' + path.relative_to(root).as_posix()] = path.read_bytes()
for name in ('Makefile', 'build.sh', 'verify.py', 'package.py', 'NOTICE.md', 'LICENSE', 'README.md'):
    files['source/NSHome/' + name] = (root / name).read_bytes()
manifest = {name: hashlib.sha256(data).hexdigest() for name, data in files.items()}
files['manifest.json'] = (json.dumps(manifest, indent=2) + '\n').encode()
destination.parent.mkdir(parents=True, exist_ok=True)
with zipfile.ZipFile(destination, 'x', compression=zipfile.ZIP_DEFLATED) as archive:
    for name, data in files.items():
        archive.writestr(name, data)
with zipfile.ZipFile(destination) as archive:
    assert archive.testzip() is None
    assert archive.read(folder + 'mitm.lst') == b'appletAE'
    assert all(hashlib.sha256(archive.read(name)).hexdigest() == digest for name, digest in manifest.items())
print(json.dumps({'path': str(destination), 'sha256': hashlib.sha256(destination.read_bytes()).hexdigest(), 'hardware_tested': False}))
