# FFmpeg Dynamic Range Compressor Module (`acompressor`)

This module ports FFmpeg's `af_sidechaincompress` downward compression algorithm into a native fixed-point SOF module for Xtensa DSPs.
It applies smooth envelope following, configurable downward ratio compression above threshold, and makeup gain.
