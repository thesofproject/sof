## Topology Switch Tool

### switch_tplg.py switch_tplg.sh
This tool allows switching to a different topology without rebooting the board. 

### How to use the tool
Copy both switch_tplg.py and switch_tplg.sh to the target hardware, ensuring they are in the same directory.

To start the UI, run the following command on the board:
```python3 switch_tplg.py```

After selecting the topology, applying it will remove and reload the relevant kernel modules, allowing the new topology to take effect without requiring a reboot.
