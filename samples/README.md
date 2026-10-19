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
HOME arming is experimental/manual. NRO and game launch examples report the
current unsupported bridge response. These samples do not use port 6801.
