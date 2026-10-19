NSHome is an independent GPL-2.0-only system module. See LICENSE.
This repository contains no hbmenu UI or hbmenu source history.

Dependencies / provenance:
- Eiffel2018/SwitchU 546903f6ec316a46d3ca8ed104771314e1a5ec19, GPL-2.0:
  remote_nro_{protocol,server,input,touch,capture}.{cpp,hpp} are adapted from
  projects/daemon/src. Namespaces, logger path, worker creation and startup
  change for this independent sysmodule. Authentication, bounded input/touch
  cleanup, screenshot format and the SWITCHU 1 wire protocol are retained.
  The launch first-account policy and delayed Breeze return are adapted from
  projects/daemon/src/{main.cpp,app_manager.cpp,breeze_launch_handoff.hpp}.
  launch_ecs.cpp adapts ecs.cpp without reinitializing the AMS runtime.
  Stock qlaunch keeps application ownership; its original replacement daemon
  is not embedded here. The new launch integration is not hardware verified.
- Atmosphere-NX/Atmosphere-libs aae2da0155a490abd3bfd17a70ccd16bb73c916a,
  GPL-2.0: IPC MITM server, original forwarding, domain and PID handling.
  No dependency source files are modified. The diagnostic uses a link wrapper
  around ServerSession::ForwardRequest. Application commands relay exactly
  once, including qlaunch's own user parameter and Start. 6801 and manual
  launch requests retain their native behavior.
- switchbrew/libnx 20cbcfd90567f7bf84df7725f27e345e2164fa6b, ISC: HIPC/CMIF
  definitions, kernel and service client functions, including the native
  user-aware debug launch request sent in the existing qlaunch context.
- switchbrew/nx-hbloader v2.4.5, commit
  82b95122c5ae8dc059bf23893ba7623c72c86773, ISC: loader/ is adapted from the
  source bundled by SwitchU 546903f6. loader/LICENSE.md retains its license.
  The fixed first target is Breeze; normal subsequent NRO replacement remains
  hbloader's loop. A consumed ticket and process/path receipt scope the launch.
- masagrator/sys-ticon 045c2a5f4500f6bcf2499122a6ee76a1a9ebe887, GPL-2.0,
  and p-sam/switch-sys-tweak: reference for selecting a qlaunch-only MITM.
  sysmodule permission metadata is adapted from that project's template.

No Nintendo code, firmware, keys, fonts or user configuration are included.

- stb_image v2.30, Sean Barrett and contributors, MIT license selected from
  its dual license. Unmodified source/stb_image.h retains the complete license.
  Upstream: https://github.com/nothings/stb (stb_image.h v2.30);
  SHA256 1f8c1b6b408f26e3b20cbfbbd4758afb3dc9b837ff1e17c258928f406148a87c.
  Only its JPEG memory decoder is compiled for startup screen validation.
