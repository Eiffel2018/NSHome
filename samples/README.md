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
checks also passed for background BP and instruction stepping. Generic NRO launch
is unsupported. These samples do not use 6801.
