Sound Dose

The purpose of this component is to calculate with DSP offload the
headphone audio playback MEL values (momentary sound exposure level).
The calculated MEL values are notified to user space to be available
every one second. The user space should respond to notification with
bytes control get to get the data. The MEL values are used to
calculate the CSD value (cumulative sound dose). Low enough CSD value
ensures the music listening is safe the user's hearing.

The calculation of MEL values and CSD values is defined in EN 50332-3
stadard. The implementation in the Android OS is described in
https://source.android.com/docs/core/audio/sound-dose .

The SOF Sound Dose component should be placed in topology as last
component before playback dai-copier.

Currently it can be tested with next test topologies:
- sof-hda-benchmark-sound_dose32.tplg
- sof-mtl-sdw-benchmark-sound_dose32-sdw0.tplg
- sof-mtl-sdw-benchmark-sound_dose32-simplejack.tplg

E.g. in the sdw topologies the controls for setting it up can be
seen with command:

$ amixer -c0 controls | grep "Jack Out Sound Dose"
numid=33,iface=MIXER,name='Jack Out Sound Dose data bytes'
numid=32,iface=MIXER,name='Jack Out Sound Dose gain bytes'
numid=30,iface=MIXER,name='Jack Out Sound Dose setup bytes'
numid=31,iface=MIXER,name='Jack Out Sound Dose volume bytes'

The above topologies program the setup bytes for acoustical
sensitivity of acoustical 100 dBSPL for 0 dBFS digital level.
The gain is set to 0 dB and volume to 0 dB.

To test the basics copy a compatible test topology over the
normal topology file. Or alternative use module options
tplg_filename and tplg_path for module snd_sof.

Get kcontrol_events from https://github.com/ujfalusi/kcontrol_events
and build and install it.

Start playback of some music or test signal. Note that there is no SOF
volume control component in the test topology, so it plays out at max
volume.

Start in other console kcontrol_events. The control 'Jack Out Sound
Dose data bytes' should get updates every one second. The structures
are defined in src/include/user/audio_feature.h and
src/include/user/sound_dose.h.

The next steps require build of blobs for sof-ctl. The blobs can be
rebuilt with command

cd src/audio/sound_dose/tune; octave --quiet --no-window-system sof_sound_dose_blobs.m

To simulate effect of initial sensitivity setup, try command

sof-ctl -c name='Jack Out Sound Dose setup bytes' -s <path>/sound_dose/setup_sens_0db.txt
sof-ctl -c name='Jack Out Sound Dose setup bytes' -s <path>/sound_dose/setup_sens_100db.txt

To simulate adjusting codec volume control down 10 dB and up again 10 dB

sof-ctl -c name='Jack Out Sound Dose volume bytes' -s ctl4/sound_dose/setup_vol_-10db.txt
sof-ctl -c name='Jack Out Sound Dose volume bytes' -s ctl4/sound_dose/setup_vol_0db.txt

To force user's listening level 10 dB down after observing too large dose per the MSD value

sof-ctl -c name='Jack Out Sound Dose gain bytes' -s ctl4/sound_dose/setup_gain_-10db.txt

To restore user's listening level to non-attenuated

sof-ctl -c name='Jack Out Sound Dose gain bytes' -s ctl4/sound_dose/setup_gain_0db.txt

The above commands have impact to data shown by kcontrol_events.
