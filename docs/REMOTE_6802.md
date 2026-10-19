# Python examples and TCP 6802 reference

This reference describes the current NSHome source (0.1.9), not
all features of other servers using the same banner. Each feature has a runnable
[Python sample](../samples/README.md). Python 3.10+ is sufficient; no pip packages
are required. The shared implementation is [tools/nshome_remote.py](../tools/nshome_remote.py).

## Connect and run

Run from the repository root, replacing `SWITCH_IP` with your console address:

```sh
python samples/01_auth.py --host SWITCH_IP
python samples/02_status.py --host SWITCH_IP
python samples/04_press.py --host SWITCH_IP --key A --ms 100
python samples/10_capture.py --host SWITCH_IP --stack 2 --output screen.jpg
python samples/08_touch_double.py --help
```

Enable Breeze's PC Connect setting and use its pairing code. Each sample prompts
for the code without displaying it; leading zeroes are preserved. No code is
stored or accepted in command-line arguments. The running sysmodule serves 6802
independently of the Breeze UI. This protocol is not TLS encrypted; use it on a
trusted local network.

Common options for **every** sample:

| Option | Required/default | Meaning |
|---|---|---|
| `--host` | Required | Switch IP address or hostname |
| `--port` | `6802`, range 1..65535 | TCP endpoint; changing it does not add another protocol |
| `--timeout` | `10` seconds, greater than 0 and at most 120 | Timeout per socket operation, not an overall job deadline |
| `--help` | Optional | Show options and exit without connecting |

Samples return exit code 0 on success and 1 on authentication, transport, remote,
or runtime validation errors. Argparse usage errors return 2. An error message
does not imply that an input or launch operation was never applied.

## Complete feature catalogue

Durations below are milliseconds. Sample defaults are convenience choices; the
wire protocol requires explicit durations. Input samples send `controller on`,
perform one operation and attempt `controller off` in `finally`, using a separate
authenticated connection for each command.

| Sample | Options specific to the sample | Wire command / result |
|---|---|---|
| [01_auth.py](../samples/01_auth.py) | None | `auth CODE` -> `OK auth`; sends no operation |
| [02_status.py](../samples/02_status.py) | None | `status` -> launch queue `id`, `state`, `rc` |
| [03_controller.py](../samples/03_controller.py) | Positional `status`, `on` or `off` | `controller ACTION` -> enabled/disabled |
| [04_press.py](../samples/04_press.py) | `--key` default A; `--ms` 1..1000, default 100 | `press KEY MS` -> released bounded button pulse |
| [05_chord.py](../samples/05_chord.py) | `--keys` default L+R; `--ms` 1..1000, default 100 | `press L+R MS`; 2..4 distinct main buttons simultaneously |
| [06_stick.py](../samples/06_stick.py) | `--side` L/R default L; required `--x`, `--y` each -32768..32767; `--ms` 1..1000 default 200 | `stick SIDE X Y MS`; returns stick to center |
| [07_touch_tap.py](../samples/07_touch_tap.py) | Required `--x` 0..1279, `--y` 0..719; `--ms` 1..300 default 100 | `touch tap X Y MS`; one contact and lift |
| [08_touch_double.py](../samples/08_touch_double.py) | Same coordinates; `--ms` 1..300 default 100; `--gap` 50..500 default 100 | `touch double X Y MS GAP`; MS per contact, GAP between contacts |
| [09_touch_long.py](../samples/09_touch_long.py) | Same coordinates; `--ms` 300..5000 default 600 | `touch long X Y MS`; one held contact and lift |
| [10_capture.py](../samples/10_capture.py) | `--stack` 0/1/2/3/4/6 default 2; required `--output` new PC file | `capture STACK`; binary JPEG, 1280x720, at most 512 KiB |
| [11_home_status.py](../samples/11_home_status.py) | None | `home status`; see fields below |
| [12_home_arm.py](../samples/12_home_arm.py) | Required `--generation` positive decimal, at most 19 digits | `home arm GENERATION`; experimental, manual identity check required |
| [13_home_off.py](../samples/13_home_off.py) | None | `home off`; stops HOME interception, module continues running |
| [14_launch_nro.py](../samples/14_launch_nro.py) | Required `--path` absolute SD .nro path | `launch PATH` -> `OK queued ID` on 0.1.9+; follow with `status` |
| [15_start_game.py](../samples/15_start_game.py) | Required `--title-id` exactly 16 hex digits, nonzero value, no 0x prefix | `gamestart TITLE_ID` -> `OK queued ID`; automatic user/Breeze flow; bounded launch, background BP/stepping and HOME checks passed |
| [16_home_button.py](../samples/16_home_button.py) | `--ms` 1..1000 default 100 | `press HOME MS`; presses HOME, does not enable HOME switching |
| [17_capture_button.py](../samples/17_capture_button.py) | `--ms` 1..1000 default 100 | `press CAPTURE MS`; console Album workflow, no JPEG response to PC |

Main buttons (single or chord): `A B X Y LSTICK RSTICK L R ZL ZR PLUS MINUS
LEFT UP RIGHT DOWN`. Other supported single buttons: `HOME CAPTURE LSL LSR RSL RSR`.
Names are uppercase. Chords cannot contain HOME, CAPTURE or rail buttons, duplicate
buttons or more than four buttons. The server does not provide indefinite
keydown/keyup, arbitrary masks or player-slot selection.

Stick X is left/right (negative/positive); Y is down/up (negative/positive).
Touch uses a 1280x720 coordinate system with (0,0) at the top left, X right, Y down.
Touch does not use a gamepad slot. Each operation releases its owned input before
replying; virtual controller setup adds latency beyond `MS`.

Capture stack values: 0 Default, 1 LCD, 2 Screenshot, 3 Recording, 4 LastFrame,
6 ApplicationForDebug. Stack 5 is rejected. A stack being accepted does not mean
it is available in every foreground/power state. `capture` without a stack also
defaults to 2. This is a single screenshot, not a video stream.

Example commands for the remaining parameter combinations:

```sh
python samples/03_controller.py --host SWITCH_IP status
python samples/03_controller.py --host SWITCH_IP off
python samples/05_chord.py --host SWITCH_IP --keys ZL+ZR+A+B --ms 150
python samples/06_stick.py --host SWITCH_IP --side R --x 16000 --y -8000 --ms 250
python samples/07_touch_tap.py --host SWITCH_IP --x 640 --y 360 --ms 100
python samples/08_touch_double.py --host SWITCH_IP --x 640 --y 360 --ms 80 --gap 120
python samples/09_touch_long.py --host SWITCH_IP --x 640 --y 360 --ms 800
python samples/11_home_status.py --host SWITCH_IP
python samples/13_home_off.py --host SWITCH_IP
python samples/16_home_button.py --host SWITCH_IP --ms 100
python samples/17_capture_button.py --host SWITCH_IP --ms 100
```

NRO path syntax accepts `/switch/Example/Example.nro` or the same path prefixed
with `sdmc:`. Spaces are preserved; quote the entire path in a shell. Canonical
UTF-8 path length must be less than 512 bytes. Traversal components, duplicate
slashes, backslashes, quotes, colons inside the path and control bytes are rejected.
NRO launch requires 0.1.9 and its matched loader; 0.1.8 rejects it. Game launch
requires 0.1.8+. Neither example falls back to another port:

```sh
python samples/14_launch_nro.py --host SWITCH_IP --path "/switch/Example/Example.nro"
python samples/15_start_game.py --host SWITCH_IP --title-id 0100000000000001
```

The title ID above is a syntax placeholder, not an installed-title recommendation.

For `launch`, the file must exist on SD. NSHome replaces the current Album NRO
internally; clients do not send Stop NRO first. Other library applets are rejected
as busy. `started` confirms the owned loader mapped the requested path, not that
its UI or network server is ready. The canonical Breeze path additionally waits
for authenticated Breeze identity. Explicit NRO launch leaves that NRO foreground
and cancels the previous automatic Breeze-return transaction. It does not close
the running game. Use `/switch/ReturnToHOME.nro` to leave the retained applet and
return to stock qlaunch.

For `gamestart`, the explicit authenticated 6802 request is the PC-origin marker.
Only that transaction selects the first system-listed user and schedules Breeze
return after the matching application's Start succeeds. Check `status` for the
same request ID: `queued`/`launching` is pending, `started` means the automatic
return reached its loader/session milestone, and `failed` includes a nonzero
result. A failed return can leave the game running; never resend a start merely
because return timed out. `started` is not proof of a game's menu being ready.
No capture/click loop or PC command to reopen Breeze is required.

Manual console launches and unmarked 6801 launches retain stock behavior.
The 6801 AM request does not prove PC origin, so NSHome does not guess from
Breeze being present. See [the candidate launch implementation](LAUNCH.md).

## Experimental HOME controls

`home status` reports `armed` (armed generation or 0), `album` (latest live Album
AM root generation or 0), `pid` (that process), `library`, `game`, `functions`
(tracked AM object IDs), and `game_front` (the bridge's internal toggle state,
not an independent foreground query).

**Album presence does not identify Breeze.** The candidate arms automatically
after the scoped PC launch verifies its Breeze loader/session (or an already
identified resident Breeze). For the separate manual diagnostic command, arm only while you have
verified Breeze is the foreground Album NRO, a game is running, and the displayed
generation is fresh. The manual sample itself does not identify Breeze.
Do not use a saved generation, and disarm before another NRO runs.

```sh
python samples/11_home_status.py --host SWITCH_IP
# Only after the checks above; replace 9 with the freshly observed album value:
python samples/12_home_arm.py --host SWITCH_IP --generation 9
python samples/13_home_off.py --host SWITCH_IP
```

`home off` leaves input/capture and the sysmodule active. `controller off` blocks
remote button/stick/touch commands and attempts cleanup; it does not stop the
module or disarm HOME interception. These controls are independent.

## Wire format, responses and failures

The server greets each connection with `SWITCHU 1\n` for protocol compatibility.
Send `auth CODE\n` and require `OK auth\n`, then submit exactly one operation
terminated by LF. CRLF is also accepted. The server closes after its operation.
The client opens a new connection and authenticates for every operation; do not
pipeline several commands or use Breeze's `+OK` framing on 6802.

Typical responses (illustrative values):

```text
OK status id=0 state=idle rc=0x00000000
OK controller enabled
OK controller disabled
OK pressed A ms=100
OK stick L x=16000 y=0 ms=200
OK touch double x=640 y=360 ms=80 gap=120
OK home armed=0 album=9 pid=138 library=50 game=40 functions=10 game_front=0
OK home disabled
OK capture bytes=12345 width=1280 height=720 format=jpeg
```

The capture header is followed immediately by **exactly `bytes` raw JPEG bytes**,
not a text line or Base64. The client bounds the size, reads short TCP fragments
until complete and checks the JPEG envelope before returning bytes. JPEG decoding
is left to the consumer. Existing output files are refused.

| Error | Meaning / handling |
|---|---|
| `ERR auth` | Pairing rejected; operation was not submitted by the sample |
| `ERR invalid-command` / `ERR invalid-path` | Request syntax rejected |
| `ERR input-disabled` | Remote input is blocked; input samples explicitly enable it |
| `ERR input rc=0x...` / `ERR touch rc=0x...` | Input service failure; no replay |
| `ERR controller-cleanup rc=0x...` | Cleanup not confirmed; inspect state before more input |
| `ERR capture rc=0x...` | Capture unavailable/failed in this state |
| `ERR home-generation` / `ERR home-not-ready` / `ERR home-command` | Invalid generation, unavailable tracked objects or unknown HOME command |
| `ERR launch-bridge-unavailable` | An older build or failed launch-worker startup; nothing queued |
| `ERR busy` | Launch queue occupied; not evidence of launch |
| Timeout / EOF / malformed reply | Outcome may be unknown; never automatically repeat input/launch |

Queue `status` is not a health check for Breeze or proof that a game/NRO appeared.
A future `OK queued ID` or `started` would only acknowledge queue/loader progress;
observe the target separately before claiming readiness.

Only one controlling client should enable input at a time: ON/OFF is module-wide,
not a per-client lease. Samples do not restore a previous ON state. Their final
OFF is a cleanup attempt, not a replay of the original operation; if it fails,
the sample reports that failure. No sample connects to 6801, reboots the console,
or falls back to another input service.

## Use the client in your Python program

Add the repository's `tools` directory to your Python import path:

```python
import getpass
from nshome_remote import Client

remote = Client("SWITCH_IP", getpass.getpass("Pairing code: "))
print(remote.status())
with remote.input_session():
    print(remote.press("A", ms=100))
    print(remote.touch("tap", x=640, y=360, ms=100))
image = remote.capture(stack=2)
with open("screen.jpg", "xb") as output:
    output.write(image)
```

Offline tests use fake sockets and local files. They verify client framing,
validation, cleanup and sample coverage; they do not establish new device support.
