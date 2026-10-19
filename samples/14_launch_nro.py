#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Queue an SD NRO (NSHome 0.1.9+); use status to check the same request ID."""
from _common import parser, run, ranged, BUTTONS, STACKS, buttons, nro_path, Path


def action(client, args):
    return client.launch(args.path)


if __name__ == '__main__':
    cli = parser(__doc__)
    cli.add_argument('--path', type=nro_path, required=True, help='Absolute SD path, e.g. /switch/NSM/NSM.nro')
    raise SystemExit(run(cli, action))
