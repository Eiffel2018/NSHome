#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Read a JPEG screenshot to a new PC file; does not save to Album."""
from _common import parser, run, ranged, BUTTONS, STACKS, buttons, nro_path, Path


def action(client, args):
    if args.output.exists():
        raise FileExistsError(args.output)
    image = client.capture(args.stack)
    # Exclusive creation also prevents a race from overwriting an existing file.
    with args.output.open('xb') as output:
        output.write(image)
    return f'Saved {len(image)} bytes to {args.output}'


if __name__ == '__main__':
    cli = parser(__doc__)
    cli.add_argument('--stack', type=int, choices=STACKS, default=2)
    cli.add_argument('--output', type=Path, required=True, help='New JPEG file; existing files are refused')
    raise SystemExit(run(cli, action))
