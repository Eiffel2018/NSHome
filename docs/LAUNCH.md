# PC game launch candidate (0.1.8)

This is a development candidate, not a published release. Stock qlaunch remains
installed and owns applications. Source `1e82420` passed a bounded device check:
automatic user selection, game launch, authenticated Breeze initialization,
return to the game and HOME round trips to the same Breeze process.

Background debugging is not accepted. Hidden Breeze `state` and `dbg` requests
returned UI busy; they recovered when that same process returned to the foreground.
No background breakpoint or continuous-trace claim follows from the launch check.

## Origin and behavior

Only an authenticated TCP6802 `gamestart TITLE_ID` creates an automatic-launch
transaction. It is bound to the request ID, qlaunch session, title and observed
application accessor. Native Start must succeed before the return timer begins.
Manual console starts never create this transaction. 6801 requests are forwarded
with their original behavior: current AM messages do not identify whether the
Breeze user clicked a menu or a PC sent `gamestart`. Breeze source is not changed.

The worker selects the first account returned by `accountListAllUsers`, matching
SwitchU's PC launch policy. An empty account list or running game is rejected
(including a read-only process query for accessors outside tracing coverage);
the module does not terminate a game to replace it. It sends the native
`IDebugFunctions::RequestLaunchApplicationWithUserAndArgumentForDebug` request
through a cloned qlaunch session, with that UID and no user launch payload.
Stock qlaunch receives its normal messages (message 50 is never swallowed),
creates the application and supplies its own selected-user launch parameter.
NSHome forwards these application commands unchanged; it does not inject a
second parameter or replace Start replies.

This adapts the old first-account and delayed-return mechanisms, rather than
copying its whole qlaunch replacement. Native qlaunch honored the user-aware
request without showing a selector in the bounded device check.

## Breeze return

After the matching Start succeeds, wait two seconds. Reuse an identified live
Breeze if available. Otherwise wait for game startup applets and other library
applets/NROs to finish; do not forcibly close another NRO. Retry availability
every two seconds, within a 30-second transaction deadline.

A dedicated nx-hbloader build loads `sdmc:/switch/Breeze/Breeze.nro` as an
Album applet. It first initializes in the foreground for compatibility, then
NSHome resumes the game after authenticated local6801 state confirms Breeze
is ready. A mapped NRO receipt alone is not sufficient. HOME is automatically
armed only for that exact live AM generation.

Its external-content registration is scoped to the owned holder; the original
qlaunch executable and persistent Album override are unchanged. A consumed
one-shot ticket plus the mapped-path/PID receipt binds the loader result to the
new session. Existing Breeze reuse also requires authenticated local6801 proof.
The first background-mode trial mapped the NRO but never exposed its endpoint;
this candidate therefore uses the foreground initialization path.

Normal subsequent NRO replacement is hbloader's own behavior. There is no
StopNRO command, forced NRO termination or generic NRO launch bridge here.
An active unexpected NRO blocks restoration instead of being closed. Launch failure is retried within the deadline. A failed automatic return does not imply the game failed.

Explicit HOME/sleep, an unrelated launch, game completion or qlaunch replacement
cancels pending return. A completed holder is released and its external-content
registration removed. Once verified, the existing HOME bridge switches between
the game and that exact resident Breeze generation without a PC `home arm` call.

## Build, artifacts and checks

`build.sh` builds both the module and `loader/`, then extracts a matched loader
`main` and `main.npdm` pair to `build/breeze_loader`. Packaging must include both.
The native request APIs require modern firmware (the candidate targets the
existing test environment); general firmware compatibility is not established.

Host tests cover PC intent versus manual starts, request/session/accessor fencing,
timing, cancellation, bounded retries and adopted HOME sessions.
Compilation verifies AArch64 module/loader
containers and program IDs. These tests do not emulate proprietary qlaunch.

The bounded device check used firmware 22.5.0, Breeze beta125.05 and one game
(0100005027A18000), stopping at its main menu without loading a save. General
firmware/game compatibility is not established. Remaining checks include multiple
accounts, unchanged manual/6801 starts, cancellation, normal exit, background
breakpoints/continuous tracing and controller behavior across process boundaries.
