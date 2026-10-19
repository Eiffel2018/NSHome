#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
import hashlib
import json
import sys
import zipfile
from pathlib import Path
from verify import verify

root = Path(__file__).resolve().parent
build = root / 'build'
verify(build / 'am-observer.elf', build / 'exefs.nsp', build / 'disassembly.txt')
destination = Path(sys.argv[1]).resolve()
if destination.exists():
    raise FileExistsError(destination)
folder = 'atmosphere/contents/00FF4E5348414D01/'
files = {
    folder + 'exefs.nsp': (build / 'exefs.nsp').read_bytes(),
    folder + 'mitm.lst': b'appletAE',
    folder + 'flags/boot2.flag': b'',
    'README.md': (root / 'README.md').read_bytes(),
    'docs/DEVELOPMENT.md': (root / 'docs/DEVELOPMENT.md').read_bytes(),
    'NOTICE.md': (root / 'NOTICE.md').read_bytes(),
    'LICENSE': (root / 'LICENSE').read_bytes(),
    'verification.json': (build / 'verification.json').read_bytes(),
}
# Include the runnable PC samples alongside their shared client and reference.
for pattern in ('samples/*.py', 'samples/*.md', 'tools/nshome_remote.py', 'docs/REMOTE_6802.md'):
    for path in root.glob(pattern):
        files[path.relative_to(root).as_posix()] = path.read_bytes()
# Include corresponding module source and the pinned dependency build recipe.
for pattern in ('source/*', 'res/*', 'tests/*.cpp', 'tests/*.sh', 'tests/*.py', 'tests/**/*.h', 'tests/**/*.hpp', 'docs/*.md', 'tools/nshome_remote.py', 'samples/*.py', 'samples/*.md'):
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
