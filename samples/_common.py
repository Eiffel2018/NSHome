# SPDX-License-Identifier: GPL-2.0-only
"""Shared CLI plumbing; no network access on import or --help."""
import argparse
import getpass
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from nshome_remote import Client, BUTTONS, STACKS, buttons, nro_path  # noqa: E402


def ranged(low, high):
    def parse(value):
        try:
            number = int(value)
        except ValueError as error:
            raise argparse.ArgumentTypeError('Expected an integer') from error
        if not low <= number <= high:
            raise argparse.ArgumentTypeError(f'Expected {low}..{high}')
        return number
    return parse


def parser(description):
    result = argparse.ArgumentParser(description=description)
    result.add_argument('--host', required=True, help='Switch IP address or hostname')
    result.add_argument('--port', type=ranged(1, 65535), default=6802, help='TCP port (default: 6802)')
    result.add_argument('--timeout', type=float, default=10, help='Per-socket timeout in seconds (default: 10)')
    return result


def run(cli, action):
    args = cli.parse_args()
    try:
        code = getpass.getpass('Breeze pairing code: ')
        client = Client(args.host, code, args.port, args.timeout)
        reply = action(client, args)
        if reply is not None:
            print(reply)
        return 0
    except (OSError, RuntimeError, ValueError, KeyboardInterrupt, EOFError) as error:
        print(f'Error: {error}', file=sys.stderr)
        return 1
