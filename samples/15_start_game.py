#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Game launch protocol example; current module returns launch-bridge-unavailable."""
from _common import parser, run, ranged, BUTTONS, STACKS, buttons, nro_path, Path


def action(client, args):
    return client.game_start(args.title_id)


if __name__ == '__main__':
    cli = parser(__doc__)
    cli.add_argument('--title-id', required=True, help='Exactly 16 hex digits; no 0x prefix')
    raise SystemExit(run(cli, action))
