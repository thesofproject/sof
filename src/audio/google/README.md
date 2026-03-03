# Google Custom Components Architecture

This directory houses components specific to Google integrations, such as specialized wake-word or hotword engines.

## Configuration and Scripts

- **Kconfig**: Exposes menus for Google components, including `COMP_GOOGLE_HOTWORD_DETECT` (engine for hotword detection), `COMP_GOOGLE_RTC_AUDIO_PROCESSING` (acoustic echo cancellation with tunable memory size, sample rates, max channels, and reference max channels), and `COMP_GOOGLE_CTC_AUDIO_PROCESSING` (crosstalk cancellation). It also exposes mock definitions for CI testing (`COMP_STUBS`).
- **CMakeLists.txt**: Conditionally links external static libraries (`libhifi3_google_hotword_dsp_api.a`, `google_rtc_audio_processing`, `google_ctc_audio_processing`) from `third_party` tools. Defines Zephyr and non-Zephyr build pipelines.
- **google_rtc_audio_processing.toml / google_ctc_audio_processing.toml**: Specify topology module configurations for `RTC_AEC` and `CTC` models respectively with pinning mappings (like DMIC pins and playback references).
- **Topology (.conf)**: Incorporates `tools/topology/topology2/include/components/ctc.conf` (defaults to UUID `bc:1b:0e:bf:6a:dc:fe:45:bc:90:25:54:cb:13:7a:b4`) and `google-rtc-aec.conf` (defaults to UUID `a6:a0:80:b7:9f:26:6f:46:b4:77:23:df:a0:5a:f7:58`) with strict audio format channel maps (e.g. 2ch vs 4ch mappings).
