# PC game launch and background Breeze (0.1.8)

This experimental release keeps stock qlaunch. It remains
installed and owns applications. Source `43c5a28` passed bounded device checks:
automatic user selection, game launch, authenticated Breeze initialization,
return to the game and HOME round trips to the same Breeze process, including
a second PC start while the previous game's retained Breeze was foreground.

Source `4e66012` additionally passed background BP and a 16-instruction trace.
The game retained foreground throughout the hit, all steps and Continue; the
temporary BP was cleared and original instructions read back unchanged. Four
HOME transitions retained the same game and Breeze processes. A new PC start
from the retained Breeze also passed in about 4.5 seconds. No save was loaded.
This verifies bounded stepping, not long-running unattended tracing.

The background-render bridge uses Breeze's existing BZOV
interactive-storage protocol (`overlay1` capability). Before giving the game
foreground, it sends EnterHidden and waits at most 1.5 seconds for its matching
Ack. This stops Breeze rendering to the hidden applet window, which otherwise
blocks its UI/debug loop. HOME back to the normal applet first uses EnterNormal.
Acknowledgements are handled by the launch worker after the AM relay responds,
without holding relay locks. Session changes invalidate pending transitions;
missing Acks disarm the pair without replaying a foreground request. The optional capability is
read only after the owned loader and authenticated Breeze identity are proven.

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

## Launch from an occupied Breeze slot

Before the handoff fix, a start from retained Breeze reached qlaunch's application
accessor but never reached Start; NSHome's tracker expired. Source `43c5a28`
passed this case on device: after stopping the first game and dismissing the
native closure dialog, one 6802 request from retained Breeze completed in about
4.4 seconds. Logs confirmed stages 79/76, 77, 61, native Start, authenticated
new Breeze generation and return to the game. Cold HOME launch also passed.

With no game running, an identified
Breeze gets one normal HOME handoff. For NSHome's own matching holder, NSHome
also requests a normal exit; it never forces termination. It waits up to five
seconds for the Album session/library slot to close and qlaunch to regain focus
before submitting the native game request. Unknown NROs or other library applets
are rejected as busy. Session changes, failed IPC or a handoff timeout prevent
submission. The process guard runs again after the handoff.

Numeric log stages 76/79 identify HOME/owned-holder exit requests; 77 confirms
handoff readiness and 78 records preflight failure. Stage 61 follows only after
preflight succeeds. No StopNRO command or generic NRO manager is added.

A tracker timeout does not withdraw an application accessor already handed to
qlaunch. It can still start after a later native UI transition. Do not replay a
failed/ambiguous start without checking the actual game process and screen.

## Breeze return

After the matching Start succeeds, wait two seconds. Reuse an identified live
Breeze if available. Otherwise wait for game startup applets and other library
applets/NROs to finish; do not forcibly close another NRO. Retry availability
every two seconds, within 30 seconds after the observed application Start.
The preceding native Start wait is separately bounded to 30 seconds.

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
accounts, unchanged manual/6801 starts, cancellation, normal exit, sleep/wake,
long-running continuous tracing and controller behavior across process boundaries.
