#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""NRO launch protocol example; current module returns launch-bridge-unavailable."""
from _common import parser, run, ranged, BUTTONS, STACKS, buttons, nro_path, Path


def action(client, args):
    return client.launch(args.path)


if __name__ == '__main__':
    cli = parser(__doc__)
    cli.add_argument('--path', type=nro_path, required=True, help='Absolute SD path, e.g. /switch/NSM/NSM.nro')
    raise SystemExit(run(cli, action))
