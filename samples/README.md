# Python 6802 samples

Start with [the complete feature and parameter reference](../docs/REMOTE_6802.md).
Each numbered Python file demonstrates one feature. Run any file with `--help`
to see its options; this never connects to the console.

```sh
python samples/02_status.py --host SWITCH_IP
python samples/04_press.py --host SWITCH_IP --key A --ms 100
python samples/10_capture.py --host SWITCH_IP --output screen.jpg
```

Python 3.10+, standard library only. The pairing code is requested privately.
Input samples enable input, perform a bounded action and attempt OFF afterward.
Manual HOME arming is a diagnostic control. Version 0.1.8 provides PC game start
with automatic user selection, Breeze return and HOME activation. Bounded device
checks also passed for background BP and instruction stepping. Version 0.1.9 adds
SD NRO launch, replacing the current Album NRO internally. Check the queued request
with sample 02; do not send another launch while its result is unknown.
These samples do not use 6801.

Sample 18 (0.1.11 development source) lists installed applications with
`--page-size 1..16` and `--wait 1..120`; see [schema and installation evidence](../docs/INSTALLED_TITLES.md).
It refreshes the read-only catalogue once and validates the same snapshot across pages.
No 6801 or console input is used; the implementation is not yet console-tested.
