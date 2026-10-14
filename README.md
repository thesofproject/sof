# Sound Open Firmware

### Status
[![Daily Actions](https://github.com/thesofproject/sof/actions/workflows/daily-tests.yml/badge.svg)](https://github.com/thesofproject/sof/actions/workflows/daily-tests.yml)
[![Zephyr Main Branch](https://github.com/thesofproject/sof/actions/workflows/zephyr-main.yml/badge.svg)](https://github.com/thesofproject/sof/actions/workflows/zephyr-main.yml)

### Community

[![Gitter chat](https://badges.gitter.im/gitterHQ/gitter.png)](https://gitter.im/thesofproject/community)

Additional community support is available via the `#sof` channel in the Zephyr Project Discord server. See the [Resources section of the @zephyrproject-rtos GitHub organization README](https://github.com/zephyrproject-rtos#resources) for Discord access information.

### Documentation

See [docs](https://thesofproject.github.io/latest/index.html)

## Quickstart

You can easily set up the complete SOF development environment, including Zephyr SDK and QEMU, by running our interactive installer script. To run the installer locally:

```bash
curl -fsSLo sdk-install.sh https://raw.githubusercontent.com/thesofproject/vscode-workspace/main/sdk-install.sh
bash sdk-install.sh
```

The script will guide you through the process of installing system dependencies, cloning the repositories, configuring Python virtual environments, and setting up the Zephyr SDK and QEMU.

## Running the tests

See [unit testing documentation](https://thesofproject.github.io/latest/developer_guides/unit_tests.html)

### Wake-on-Voice (WoV) Multi-Slot Hardware Verification

For automated hardware testing of the 4-channel native 16 kHz DMIC Wake-on-Voice pipeline with multi-slot microWakeWord (MWW) on Intel Panther Lake (PTL), see the comprehensive runbook:
* **Documentation & Reproduction Guide**: [Wake-on-Voice (WoV) S0 / D0i3 Multi-Slot Testing & Verification](tools/topology/topology2/README.md#wake-on-voice-wov-s0--d0i3-multi-slot-testing--verification)
* **Required Kernel Branch**: `wov-ipc4-d0i3` on [`lgirdwood/linux`](https://github.com/lgirdwood/linux/tree/wov-ipc4-d0i3) (`SNDRV_PCM_INFO_NO_PERIOD_WAKEUP` + `SOF_IPC4_NOTIFY_PHRASE_DETECTED`)
* **Required Firmware Branch**: `wcl-uaol-wov-002` on [`lgirdwood/sof`](https://github.com/lgirdwood/sof/tree/wcl-uaol-wov-002)
* **Verified Results**: 20 / 20 consecutive passes in S0 mode with synthetic fake-wake detection (`CONFIG_COMP_MWW_FAKE_WAKE_MS=5000`), zero xruns, and zero kernel IPC/ASoC errors.

## Deployment

TODO: Add additional notes about how to deploy this on a live system

## Contributing

See [Contributing to the Project](https://thesofproject.github.io/latest/contribute/index.html)

## License

This project is licensed under the BSD Clause 3 - see the [LICENCE](LICENCE) file for details
