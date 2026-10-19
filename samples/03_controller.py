#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Inspect, enable or disable module-wide remote input."""
from _common import parser, run, ranged, BUTTONS, STACKS, buttons, nro_path, Path


def action(client, args):
    return client.controller(args.action)


if __name__ == '__main__':
    cli = parser(__doc__)
    cli.add_argument('action', choices=('status', 'on', 'off'))
    raise SystemExit(run(cli, action))
