## sof-demo-gui

### sof-demo-gui.py - sof-demo-tui.py
User input logic and display handling

### sof-controller-engine
Controller to abstract the GUI and TUI control.

Handles the linking of user input and sof_ctl generically of the control type.

### How to use the interfaces

Build sof-ctl for target and copy it to the gui folder base directory within your local repo.

If you have audio on your local machine that you wish to demonstrate using the GUI, add it to the audios subfolder.
Also, you can specify audio paths on the target with the command line arg --audio-path "path"

If you would like to include eq configs, they are stored in tools/ctl/ipc3. Copy them from there to the eq_configs folder.

Copy entire GUI folder to target hardware.

Next, ensure that the
```sof-<hardware-name>-gui-components-wm8960.tplg```
is built and loaded as SOF's topology. Make sure this is built for your target hardware.

After this, run either the GUI or TUI on a board with SOF loaded. This can be done using the command:
```python3 sof-demo-gui```
or
```python3 sof-demo-tui```

The interfaces themselves are self-explanatory, as they are made to be plug and play on all SOF supporting systems.

The features currently supported are:
Playback and Record with ALSA
Volume control using the SOF component
EQ component with realtime control
Generic implementation with autodetection of SOF cards and commands
