## Developing sof-gui and tui

The architecture of the SOF UIs is simple, and designed to make implementing new features extremely easy.

Controller engine "sof_controller_engine.py", that handles all interaction with Linux and SOF.

GUI "sof_demo_gui.py", links a GTK gui with the controller engine.

TUI "sof_demo_tui.py", links a text UI to the controller engine.

eq_configs and audios folders to contain example audios and EQ commands.

Pipeline within topology folder, named:
```sof-<hardware-name>-gui-components-wm8960.tplg```

## Adding a new component to the UIs

There are three main things that need to be edited to add a new component:

### Controller engine

Provide required sof_ctl calls and other necessary logic to control the component. Update execute_command to contain the desired commands. Ensure that autodetection is used for commands so that the implementation is generic.

### GUI and TUI

Add new buttons to the init method or gui that provide the needed functionality for the component. These should be designed to call methods in controller engine that will then interact with SOF and Linux

### Pipeline

See relevant documentation for pipeline development. Ensure any control needed is exposed through the pipeline. Also ensure the pipeline is set to build for your target HW within the cmakefiles.

## Next steps for overall UI development

Add DRC and other base level SOF components.

Add real time EQ config generation, so the user could control low, mid, and high controls in real time. This would require a new EQ component that supports smooth real time control.

Add graphics and other quality of life functions to the GUI.

Create a version of sof-ctl that provides direct Python bindings to communicate with SOF components, rather than needing a Linux command.
