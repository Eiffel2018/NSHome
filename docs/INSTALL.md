# NSHome 0.1.9 installation

Experimental release, tested on one console with Breeze beta125.05 and gen2 v0.20.
Other firmware/game combinations are not yet verified.

## Requirements and installation

1. Use an existing Atmosphere setup with original Nintendo qlaunch. Disable any
   SwitchU HOME daemon/replacement before installing NSHome, then reboot.
2. Install [Breeze](https://github.com/tomvita/Breeze-Beta/releases/tag/beta125.05)
   separately at `switch/Breeze/Breeze.nro`. Enable PC connect in its Settings
   and choose your own pairing code. NSHome reads the same `[PC connect]`
   settings from `switch/Breeze/config.ini`; restart after changing them.
3. Copy the ZIP's `atmosphere` and `switch` folders to the SD root and reboot.
   Original Nintendo HOME should appear. The module is
   `atmosphere/contents/00FF4E5348414D01/exefs.nsp`; `flags/boot2.flag` enables it.
4. For BP/trace, install Breeze's compatible gen2 fork using Breeze Settings.
   It is a separate dependency, not part of this ZIP. Do not enable/install the
   SwitchU HOME daemon for NSHome. The tested Breeze advertises `overlay1 overlay2`.

The ZIP includes the matched Breeze loader, module source and build recipe,
Python client and samples. It does not include Breeze, gen2, Atmosphere,
firmware or pairing settings.

## Start a game and switch HOME

From the extracted ZIP, use Python 3.10+:

```sh
python samples/15_start_game.py --host SWITCH_IP --title-id YOUR_16_DIGIT_TITLE_ID
```

Enter the pairing code at the private prompt. Use an installed game title ID.
NSHome selects the first system-listed user, starts the game, initializes Breeze
and returns to the game. HOME switches to that same Breeze and back. No manual
`home arm` or polling script to reopen Breeze is needed.

Start with no game running. The current Album NRO is replaced internally;
unrelated library applets are rejected as busy. Manual console/6801 game starts
keep their native behavior. Use sample 14 with `--path /switch/Breeze/Breeze.nro`
to open Breeze directly, or another existing SD NRO path. Follow the queued request
with sample 02 rather than resending it.
[6802 samples and parameters](REMOTE_6802.md) cover all commands.

## Background BP and trace

Use Breeze's normal debugging tools/6801 API. NSHome keeps Breeze processing
while the game owns foreground; Breeze can show its own overlay on a BP hit.
When HOME brings the normal full-screen Breeze forward, qlaunch pauses the game.
Return to the game before stepping instructions. Clear your breakpoints and
resume the game before exiting or rebooting.

## Optional boot unlock

Copy the inner `config` folder from `extras/auto-unlock` to the SD root to opt in.
One visually checked touch is attempted at boot, with no input retry. To disable
it, remove `config/NSHome/boot-touch-once.flag`. Remove `skip-lock-screen.flag`
too and restore the lock setting in System Settings if desired; removing flags
alone does not restore that setting. Other boot screen layouts are unverified.

## Returning to stock HOME

Known issue: after leaving the owned Breeze for hbmenu, HOME and `+` may fail
to return to qlaunch. `home off` only disables HOME interception; it does not
close the retained applet. The optional [Return to HOME NRO](../tools/return_to_home/README.md)
uses the library-applet exit path. It passed a device check from NSHome's
retained hbmenu to stock qlaunch and System Settings, with the game retained.
Version 0.1.9 includes the tool under `switch/`; copy that folder to the SD
root too. Reboot remains a
recovery option if the tool is unavailable.

## Limits and removal

Tests cover main-menu background BP/stepping, continue, HOME round trips, cold
launch and retained-Breeze relaunch. A ten-minute trace completed, including
resolution of two late UI replies; it mostly remained in one instruction loop.
Sleep/wake, other game applets and general exit/chainload remain unverified.
See [validation limits](LAUNCH.md).
The existing 6801 Stop game command can display Nintendo's software-closed dialog.
Never replay an input or launch whose outcome is unknown.

Logs are under `config/NSHome/`. To uninstall or recover from a boot problem,
power off and remove `atmosphere/contents/00FF4E5348414D01` using SD access.
