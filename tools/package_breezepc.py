#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Package the separate GPL-3.0 desktop companion, without console/user files."""
import hashlib
import json
from pathlib import Path
import sys
import zipfile


def package(destination):
    folder = Path(__file__).resolve().parent / 'breezepc'
    files = {p.name: p.read_bytes() for p in folder.iterdir()
             if p.is_file() and (p.suffix in ('.py', '.bat', '.md', '.json') or p.name == 'LICENSE')}
    assert {'pcconnect_app.py', 'pcconnect.py', 'switchu_remote.py',
            'pc_game_history.py', 'LICENSE', 'README.md'} <= files.keys()
    for name, data in files.items():
        if name.endswith('.py'):
            compile(data, name, 'exec')
    manifest = {name: hashlib.sha256(data).hexdigest() for name, data in files.items()}
    files['manifest.json'] = (json.dumps(manifest, indent=2) + '\n').encode()
    destination = Path(destination).resolve()
    destination.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(destination, 'x', zipfile.ZIP_DEFLATED) as archive:
        for name, data in sorted(files.items()):
            archive.writestr(name, data)
    with zipfile.ZipFile(destination) as archive:
        assert archive.testzip() is None
        assert all(archive.read(name) == data for name, data in files.items())
    print(json.dumps({'path': str(destination), 'sha256': hashlib.sha256(destination.read_bytes()).hexdigest()}))


if __name__ == '__main__':
    package(sys.argv[1])
