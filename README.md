# NSHome

An independent Nintendo Switch system module for retaining original qlaunch
while switching HOME between a running game and a resident Breeze NRO.
Experimental version: **0.1.7**. This is a new standalone repository, with
no hbmenu UI, assets or inherited Git history.

## Current behavior

- Original qlaunch remains installed; module Program ID: `00FF4E5348414D01`.
- Authenticated TCP 6802 provides controls, touch, capture and diagnostics.
- One automatic cold-boot touch unlock passed device verification.
- Manually armed HOME switching retained the same Breeze session; a background
  breakpoint hit and a pending single step were verified on the test console.

This is not a finished HOME replacement. Automatic Breeze identification/arming,
continuous trace and general exit/chainload behavior remain unfinished. Breeze
UI/debug RPC can return busy while hidden. Stock qlaunch suspends the game while
Breeze is in front. The 6802 NRO/game launch bridge is not implemented.

## Build and test

Host tests (Linux, g++ with ASan/UBSan):

```sh
bash tests/run.sh
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
