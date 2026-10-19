# NSHome 0.1.8 installation

Experimental release, tested on one console with Breeze beta125.05 and gen2 v0.20.
Other firmware/game combinations are not yet verified.

## Requirements and installation

1. Use an existing Atmosphere setup with original Nintendo qlaunch. Disable any
   SwitchU HOME daemon/replacement before installing NSHome, then reboot.
2. Install [Breeze](https://github.com/tomvita/Breeze-Beta/releases/tag/beta125.05)
   separately at `switch/Breeze/Breeze.nro`. Enable PC connect in its Settings
   and choose your own pairing code. NSHome reads the same `[PC connect]`
   settings from `switch/Breeze/config.ini`; restart after changing them.
3. Copy only the ZIP's `atmosphere` folder to the SD root and reboot.
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

Start with no game running. An identified retained Breeze can be handed back
normally before the next PC start; unrelated active applets are rejected as busy.
Manual console/6801 game starts keep their native behavior. Generic 6802 NRO
launch is unsupported. [6802 samples and parameters](REMOTE_6802.md) cover all commands.

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
close the retained applet. If trapped, reboot and change system settings before
starting another PC game session. A reliable return-to-qlaunch path is pending.

## Limits and removal

Only bounded main-menu tests have passed: background BP, 16 instruction steps,
continue, HOME round trips, cold launch and retained-Breeze relaunch. Long-running
traces, sleep/wake, other game applets and general exit/chainload need more testing.
An uninterrupted step-into endurance probe returned UI busy after 529 instructions;
unrestricted long traces have not passed. See [the observed failure](LAUNCH.md).
The existing 6801 Stop game command can display Nintendo's software-closed dialog.
Never replay an input or launch whose outcome is unknown.

Logs are under `config/NSHome/`. To uninstall or recover from a boot problem,
power off and remove `atmosphere/contents/00FF4E5348414D01` using SD access.
