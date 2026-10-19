# NSHome

An independent Nintendo Switch system module for retaining original qlaunch
while switching HOME between a running game and a resident Breeze NRO.
Experimental version: **0.1.8 development candidate** (not released; background debugging is not accepted).
Bounded game-launch and HOME-switching checks passed on one test console. This is a standalone repository, with
no hbmenu UI, assets or inherited Git history.

## Current behavior

- Original qlaunch remains installed; module Program ID: `00FF4E5348414D01`.
- Authenticated TCP 6802 provides controls, touch, capture and diagnostics.
- The development candidate adds PC6802 game launch with automatic first-user
  selection, delayed Breeze return and HOME arming. Manual/6801 starts retain
  their original behavior. See [the launch implementation and pending checks](docs/LAUNCH.md).
- One automatic cold-boot touch unlock passed device verification.
- Automatic PC6802 launch selected a user, restored Breeze, resumed the game,
  and retained the same Breeze process across HOME round trips.
- Earlier 0.1.7 manually armed tests included a background breakpoint hit and
  pending single step; those results do not validate the new automatic flow.

This is not a finished HOME replacement. Background debugging, continuous trace,
manual-launch regression and general exit/chainload behavior still need validation.
In the tested automatic flow, hidden Breeze `state`/`dbg` RPC returned busy;
these calls recovered when HOME brought the same Breeze back into focus. Stock qlaunch suspends the game while
Breeze is in front. Generic 6802 NRO launch remains unsupported.

## Python examples

See the [complete TCP 6802 feature and parameter reference](docs/REMOTE_6802.md)
and [17 runnable Python samples](samples/README.md). They cover authentication,
status, input controls, buttons/chords, sticks, touch, JPEG capture and experimental
HOME controls. Game launch describes the new candidate; generic NRO launch is unsupported.
Python 3.10+ and its standard library are sufficient.

## Build and test

Host tests (Linux, g++ with ASan/UBSan):

```sh
bash tests/run.sh
python3 -m unittest discover -s tests -p '*_test.py'
```

Switch build requires devkitPro/devkita64, libnx
`20cbcfd90567f7bf84df7725f27e345e2164fa6b` and Atmosphere-libs
`aae2da0155a490abd3bfd17a70ccd16bb73c916a`:

```sh
export AMS_LIBRARIES=/absolute/path/to/Atmosphere-libs
bash build.sh
python3 package.py /absolute/path/to/NSHome.zip
```

See [development and installation notes](docs/DEVELOPMENT.md) for build-image
pins, experimental arming, boot flags and recovery. Disable any SwitchU qlaunch
override before installing this module. 

## License

[GPL-2.0-only](LICENSE). Specific SwitchU mechanisms and other dependencies
are credited in [NOTICE.md](NOTICE.md); their original licenses are retained.
No Nintendo code, firmware, keys, fonts or pairing configuration is included.
