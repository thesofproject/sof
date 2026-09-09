# FFmpeg Stereo Widener Module (`stereowiden`)

This module ports FFmpeg's `af_stereowiden` algorithm into a native fixed-point SOF module for Xtensa DSPs.
It broadens the stereo image of audio streams via mid/side crossfeed and feedback.
