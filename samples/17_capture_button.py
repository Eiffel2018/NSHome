#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Press CAPTURE to save through the console Album workflow."""
from _common import parser, run, ranged, BUTTONS, STACKS, buttons, nro_path, Path


def action(client, args):
    with client.input_session():
        return client.press('CAPTURE', args.ms)


if __name__ == '__main__':
    cli = parser(__doc__)
    cli.add_argument('--ms', type=ranged(1, 1000), default=100)
    raise SystemExit(run(cli, action))
