SOF audio quality test scripts

1. Introduction

The purpose of the script is to test playback (and/or recording
quality of a SOF platform with help of PC with a high quality USB
sound card. Or in other words a sound card with sufficient number of
analog inputs and outputs with sufficient signal levels handling
capability, and good analog signal quality. The sound card should be
better than expected quality of the SOF device.

+------+                                      +--------+
|      |<--- Ethernet or WLAN connection ---->|        |
|  PC  |             +-------+                | SOF    |
|      |<--- usb --->| Sound |--- line in --->| device |
+------+             | card  |<-- line out ---|        |
	  	     +-------+                +--------+

The tests can also use digital interfaces like S/PDIF if available in
the SOF device. That helps to move testing focus to smaller digital
signal processing issues instead of usually dominant ADC/DAC
performance (unless there's a major quality issue).

The key tests to execute to quickly verify playback/recording audio
quality are gain, frequency response (FR), and total harmonic
distortion plus noise (THD+N). The test procedure and results reporting
tries to follow AES17 recommendations.

This work can be taken as a inexpensive introduction to objective
audio quality parameters testing that should be useful for individual
developers without access to professional test equipment for relative
performance indicators testing. Tests like THD+N also work for
automatic testing needs to quickly flag regressions in playback audio
integrity.

IMPORTANT NOTE: These test scripts are not calibrated and not suitable
to produce absolute metric. The results will vary depending on the
used sound card and depend on correctly done preparations on the
PC. Therefore for professional usage with need for industry comparable
metric we recommend instead dedicated test systems and services.


2. General preparations

The PC should be installed with a Linux distribution that provides
octave and octave-signal packaces or use commercial Matlab (R)
software. For Octave it's recommended to use an initialization script
that loads the needed packages and disables the pager to freely scroll
the test text printings without frequently stopping to press <space>
to proceed.

$ sudo apt-get install octave octave-signal octave-io

$ cat ~/.octaverc
more off
pkg load signal
pkg load io

The SOF device should be made accessible via ssh without
password. Also the test user should belong to group audio in both PC
and SOF device.

To avoid including the PC audio server and it's signal processing into
the test chain it is recommended to disable audio servers like
pulseaudio permanently (rename the executable /usr/bin/pulseaudio) on
the test PC and use the ALSA provided sound devices directly.

There is need to go trough the alsamixer settings for the sound card
and store them persistently. Also on sophisticated sound cards need to
do internal settings via front panel or dedicated control SW for
settings those are not exposed to alsamixer.


3. Playback and recording test preparations

See script sof_audio_quality_test_config.m. It is a template to
configure the audio interfaces for test playback and capture of the
playback. Make a copy of the file for your test setup and edit the
script sof_audio_quality_test_top.m to use that configuration script
instead of the default. See line

configs = {'sof_audio_quality_test_config.m'};

The line initializes a cell array of strings. Multiple test
configurations are added to cell array with comma as separator.

To test playback of SOF device there's need to edit "play.user" to
correpond to actual test user and hostname or IP address of the
device. Also edit the "play.dev" to contain the audio playback device
to test.

For playabck capture to PC edit the "rec.dev", "rec.sft", "rec.nch",
and "rec.ch" fields. The example is set for a 8ch sound card with SOF
device connected into analog inputs 1 and 2.

The setting "test.att_rec_db" is important. There must be a sufficient
analog signal headroom for the sound card ADC to capture SOF DAC
playback signal without clipping distortion. The example assumes 3 dB
attenuation but it can be adjusted to be e.g. within 1 - 6 dB.

You can start with 3 dB and run the test script
sof_audio_quality_test.sh. It won't proceed if the gain test is not
passing. Based on the reported difference vs. expected 0 dB gain
adjust the alsamixer settings for you USB sound card capture gain. The
controls may be also in the front panel.

If the result still deviates from 0 dB and it's known that the
playback level is correct it is possible to to adjust this attenuation
parameter to get the 0 dB result.

4. Tests reporting

The script is configured to proceed without opening plot windows to
desktop. Once the gain test is passed the script executes the FR and
THD+N tests.

The shell window will show a brief test report and the same will
appear to directory "reports" with name prefix defined in "test.id" in
the configuration file.

The plots in PNG format are exported to directory "plots".

That's all, happy testing!!
