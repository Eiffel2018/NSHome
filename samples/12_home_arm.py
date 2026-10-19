#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Manually arm experimental HOME switching; Breeze must be in front."""
from _common import parser, run, ranged, BUTTONS, STACKS, buttons, nro_path, Path


def action(client, args):
    return client.home_arm(args.generation)


if __name__ == '__main__':
    cli = parser(__doc__)
    cli.add_argument('--generation', type=ranged(1, 9999999999999999999), required=True, help='Fresh album value from home status; operator must identify Breeze')
    raise SystemExit(run(cli, action))
