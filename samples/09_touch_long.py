#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Hold one screen contact, then lift."""
from _common import parser, run, ranged, BUTTONS, STACKS, buttons, nro_path, Path


def action(client, args):
    with client.input_session():
        return client.touch('long', args.x, args.y, args.ms)


if __name__ == '__main__':
    cli = parser(__doc__)
    cli.add_argument('--x', type=ranged(0, 1279), required=True)
    cli.add_argument('--y', type=ranged(0, 719), required=True)
    cli.add_argument('--ms', type=ranged(300, 5000), default=600)
    raise SystemExit(run(cli, action))
