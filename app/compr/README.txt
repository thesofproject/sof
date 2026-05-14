Cadence Codec Configuration Overlays
=====================================

This directory contains Kconfig overlay files for enabling Cadence codec support
in SOF firmware builds.

Prerequisites
-------------
The Cadence codec libraries must be placed in the cadence_libs directory, one level
up from the SOF project directory:
  sof/../cadence_libs/

Available Overlay Files
-----------------------
cadence.conf      - Base Cadence codec module only (no individual codecs)
mp3.conf          - MP3 decoder and encoder
aac.conf          - AAC decoder
vorbis.conf       - Vorbis decoder
pcm.conf	  - PCM (wav) decoder
all_codecs.conf   - All supported codecs (MP3, AAC, Vorbis, pcm)

Usage Examples
--------------
Use with scripts/xtensa-build-zephyr.py via -o parameter:

# Base module only (e.g., for Tiger Lake)
scripts/xtensa-build-zephyr.py tgl -o app/compr/cadence.conf

# Single codec
scripts/xtensa-build-zephyr.py mtl -o app/compr/mp3.conf

# Multiple codecs (use separate -o for each)
scripts/xtensa-build-zephyr.py mtl -o app/compr/mp3.conf -o app/compr/aac.conf

# All codecs
scripts/xtensa-build-zephyr.py mtl -o app/compr/all_codecs.conf

For more information about Cadence codecs, see the SOF documentation.
