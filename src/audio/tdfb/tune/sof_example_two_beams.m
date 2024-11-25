function sof_example_two_beams()

% example_two_beams()
%
% Creates configuration files for a two beams design, one
% points to -90 or -25 degrees and other to +90 or +25 degrees
% direction for 50 mm spaced two microphones configuration. The
% beams are output to stereo and left right channels.
%
% The four channels version is for 28 mm mic spacing. The
% first beam is copied to channels 1 and 3. The second
% beam is copied to channels 2 and 4.

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2020, Intel Corporation. All rights reserved.
%
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

%% Stereo capture blobs with two beams
az = [0 30 90];
azstr = az_to_string(az);

prm.export_note = 'Created with script example_two_beams.m';
prm.export_howto = 'cd tools/tune/tdfb; matlab -nodisplay -nosplash -nodesktop -r example_two_beams';
prm.type = 'SDB';
prm.add_beam_off = 1;

for fs = [16e3 48e3]
	%% Close all plots to avoid issues with large number of windows
	close all;

	%% 2 mic 50 mm array
	fn.tplg1_fn = sprintf('coef_line2_50mm_pm%sdeg_%dkhz.m4', azstr, fs/1e3);
	fn.sofctl3_fn = sprintf('coef_line2_50mm_pm%sdeg_%dkhz.txt', azstr, fs/1e3);
	fn.tplg2_fn = sprintf('line2_50mm_pm%sdeg_%dkhz.conf', azstr, fs/1e3);
	fn.sofctl4_fn = sprintf('line2_50mm_pm%sdeg_%dkhz.txt', azstr, fs/1e3);
	d = 50e-3;  % 50 mm spacing
	a1 = az;   % Azimuth +az deg
	a2 = -az;  % Azimuth -az deg
	sof_bf_line2_two_beams(fs, d, a1, a2, fn, prm);

	%% 2 mic 68 mm array
	fn.tplg1_fn = sprintf('coef_line2_68mm_pm%sdeg_%dkhz.m4', azstr, fs/1e3);
	fn.sofctl3_fn = sprintf('coef_line2_68mm_pm%sdeg_%dkhz.txt', azstr, fs/1e3);
	fn.tplg2_fn = sprintf('line2_68mm_pm%sdeg_%dkhz.conf', azstr, fs/1e3);
	fn.sofctl4_fn = sprintf('line2_68mm_pm%sdeg_%dkhz.txt', azstr, fs/1e3);
	d = 68e-3;  % 68 mm spacing
	a1 = az;   % Azimuth +az deg
	a2 = -az;  % Azimuth -az deg
	sof_bf_line2_two_beams(fs, d, a1, a2, fn, prm);

	%% 2 mic 73.5 mm array
	fn.tplg1_fn = sprintf('coef_line2_74mm_pm%sdeg_%dkhz.m4', azstr, fs/1e3);
	fn.sofctl3_fn = sprintf('coef_line2_74mm_pm%sdeg_%dkhz.txt', azstr, fs/1e3);
	fn.tplg2_fn = sprintf('line2_74mm_pm%sdeg_%dkhz.conf', azstr, fs/1e3);
	fn.sofctl4_fn = sprintf('line2_74mm_pm%sdeg_%dkhz.txt', azstr, fs/1e3);
	d = 73.5e-3;  % 73.5 mm spacing
	a1 = az;   % Azimuth +az deg
	a2 = -az;  % Azimuth -az deg
	sof_bf_line2_two_beams(fs, d, a1, a2, fn, prm);

	%% 4 mic 28 mm spaced array
	fn.tplg1_fn = sprintf('coef_line4_28mm_pm%sdeg_%dkhz.m4', azstr, fs/1e3);
	fn.sofctl3_fn = sprintf('coef_line4_28mm_pm%sdeg_%dkhz.txt', azstr, fs/1e3);
	fn.tplg2_fn = sprintf('line4_28mm_pm%sdeg_%dkhz.conf', azstr, fs/1e3);
	fn.sofctl4_fn = sprintf('line4_28mm_pm%sdeg_%dkhz.txt', azstr, fs/1e3);
	d = 28e-3;  % 28 mm spacing
	a1 = az;   % Azimuth +az deg
	a2 = -az;  % Azimuth -az deg
	sof_bf_line4_two_beams(fs, d, a1, a2, fn, prm);

	%% 4 mic 68 mm spaced array
	fn.tplg1_fn = sprintf('coef_line4_68mm_pm%sdeg_%dkhz.m4', azstr, fs/1e3);
	fn.sofctl3_fn = sprintf('coef_line4_68mm_pm%sdeg_%dkhz.txt', azstr, fs/1e3);
	fn.tplg2_fn = sprintf('line4_68mm_pm%sdeg_%dkhz.conf', azstr, fs/1e3);
	fn.sofctl4_fn = sprintf('line4_68mm_pm%sdeg_%dkhz.txt', azstr, fs/1e3);
	d = 68e-3;  % 68 mm spacing
	a1 = az;   % Azimuth +az deg
	a2 = -az;  % Azimuth -az deg
	sof_bf_line4_two_beams(fs, d, a1, a2, fn, prm);

	%% 4 mic 78 mm spaced array
	fn.tplg1_fn = sprintf('coef_line4_78mm_pm%sdeg_%dkhz.m4', azstr, fs/1e3);
	fn.sofctl3_fn = sprintf('coef_line4_78mm_pm%sdeg_%dkhz.txt', azstr, fs/1e3);
	fn.tplg2_fn = sprintf('line4_78mm_pm%sdeg_%dkhz.conf', azstr, fs/1e3);
	fn.sofctl4_fn = sprintf('line4_78mm_pm%sdeg_%dkhz.txt', azstr, fs/1e3);
	d = 78e-3;  % 78 mm spacing
	a1 = az;   % Azimuth +az deg
	a2 = -az;  % Azimuth -az deg
	sof_bf_line4_two_beams(fs, d, a1, a2, fn, prm);
end

%% Export blob with just +/- 90 deg beams for testbench beampattern check
close all;
az = [90];
azstr = az_to_string(az);
prm.add_beam_off = 0;
for fs = [16e3 48e3]
	%% 2 mic 50 mm array, disable beam off description in blob to force processing on
	fn.tplg1_fn = sprintf('coef_line2_50mm_pm%sdeg_%dkhz.m4', azstr, fs/1e3);
	fn.sofctl3_fn = sprintf('coef_line2_50mm_pm%sdeg_%dkhz.txt', azstr, fs/1e3);
	fn.tplg2_fn = sprintf('line2_50mm_pm%sdeg_%dkhz.conf', azstr, fs/1e3);
	fn.sofctl4_fn = sprintf('line2_50mm_pm%sdeg_%dkhz.txt', azstr, fs/1e3);
	d = 50e-3;  % 50 mm spacing
	a1 = az;   % Azimuth +az deg
	a2 = -az;  % Azimuth -az deg
	sof_bf_line2_two_beams(fs, d, a1, a2, fn, prm);

	%% 4 mic 28 mm spaced array, no beam off configuration
	fn.tplg1_fn = sprintf('coef_line4_28mm_pm%sdeg_%dkhz.m4', azstr, fs/1e3);
	fn.sofctl3_fn = sprintf('coef_line4_28mm_pm%sdeg_%dkhz.txt', azstr, fs/1e3);
	fn.tplg2_fn = sprintf('line4_28mm_pm%sdeg_%dkhz.conf', azstr, fs/1e3);
	fn.sofctl4_fn = sprintf('line4_28mm_pm%sdeg_%dkhz.txt', azstr, fs/1e3);
	d = 28e-3;  % 28 mm spacing
	a1 = az;   % Azimuth +az deg
	a2 = -az;  % Azimuth -az deg
	sof_bf_line4_two_beams(fs, d, a1, a2, fn, prm);
end

%% Circular array with two beams
az = 30;
azstr = az_to_string(az);
for fs = [48e3 16e3]
	fn.tplg1_fn = sprintf('coef_circular8_100mm_pm%sdeg_%dkhz.m4', azstr, fs/1e3);
	fn.sofctl3_fn = sprintf('coef_circular8_100mm_pm%sdeg_%dkhz.txt', azstr, fs/1e3);
	fn.tplg2_fn = sprintf('coef_circular8_100mm_pm%sdeg_%dkhz.conf', azstr, fs/1e3);
	fn.sofctl4_fn = sprintf('coef_circular8_100mm_pm%sdeg_%dkhz.txt', azstr, fs/1e3);
	circular_two_beams(fs, 100e-3, 8, az, -az, fn, 0);
end

%% Creates beamformer with two beams for device with device with microphones
%  at 0, 36, 146, 182mm locations
line_xyz(16e3);
line_xyz(48e3);

end

function s = az_to_string(az)
	s = sprintf('%d', az(1));
	for n = 2:length(az)
		s = sprintf('%s_%d', s, az(n));
	end
end

function circular_two_beams(fs, r, n, a1, a2, fn, add_beam_off)

% Get defaults and common settings
bf1 = sof_bf_defaults();
bf1.beam_off_defined = add_beam_off;
bf1.fs = fs;
bf1.mic_r = r;
bf1.mic_n = n;
bf1.array = 'circular';              % Calculate xyz coordinates for circular
bf1.output_stream_mix = zeros(1, n); % No support yet
bf1.num_output_channels = 2;         % Two channels
bf1.num_output_streams = 1;          % One sink stream
bf1.input_channel_select = 0:(n-1);  % Input all n channels to filters

% Copy settings for bf2
bf2 = bf1;

% Design beamformer 1 (left)
bf1.output_channel_mix_beam_off = zeros(1, n); % For some stereo symmetry
bf1.output_channel_mix_beam_off(2) = 1;        % Mic2 to channel 2^0
bf1.output_channel_mix_beam_off(n) = 2;        % Mic2 to channel 2^1
bf1.output_channel_mix = 1 * ones(1, n);       % Mix all filters to channel 1
bf1.steer_az = a1;
bf1 = sof_bf_filenames_helper(bf1);
bf1 = sof_bf_design(bf1);

% Design beamformer 2 (right)
bf2.output_channel_mix_beam_off = zeros(1, n); % No channels input
bf2.output_channel_mix = 2 * ones(1, n);       % Mix all filters to channel 2
bf2.steer_az = a2;
bf2 = sof_bf_filenames_helper(bf2);
bf2 = sof_bf_design(bf2);

% Merge two beamformers into single description, set file names
bfm = sof_bf_merge(bf1, bf2);
bfm.sofctl3_fn = fullfile(bfm.sofctl3_path, fn.sofctl3_fn);
bfm.tplg1_fn = fullfile(bfm.tplg1_path, fn.tplg1_fn);
bfm.sofctl4_fn = fullfile(bfm.sofctl4_path, fn.sofctl4_fn);
bfm.tplg2_fn = fullfile(bfm.tplg2_path, fn.tplg2_fn);

% Export files for topology and sof-ctl
bfm.export_note = 'Created with script sof_example_two_beams.m';
bfm.export_howto = 'cd tools/tune/tdfb; matlab -nodisplay -nosplash -nodesktop -r sof_example_two_beams';
sof_bf_export(bfm);

end

function line_xyz(fs)

% Get defaults
bf1 = sof_bf_defaults();
bf1.fs = fs;
bf1.beta = 5;

% The large size array needs more taps for 48 kHz, also the blob needs to
% be less than 4 kB.
switch fs
	case 16000
		az = [0 30 90];
		azstr = 'pm0_30_90deg';
		bf1.fir_length = 64;
	case 48000
		az = [0 30];
		azstr = 'pm0_30deg';
		bf1.fir_length = 100;
end

% Setup array
tplg1_fn = sprintf('coef_line4_0mm36mm146mm182mm_%s_%dkhz.m4', azstr, fs/1e3);
sofctl3_fn = sprintf('coef_line4_0mm36mm146mm182mm_%s_%dkhz.txt', azstr, fs/1e3);
tplg2_fn = sprintf('line4_0mm36mm146mm182mm_%s_%dkhz.conf', azstr, fs/1e3);
sofctl4_fn = sprintf('line4_0mm36mm146mm182mm_%s_%dkhz.txt', azstr, fs/1e3);
a1 = az;   % Azimuth +az deg
a2 = -az;  % Azimuth -az deg
close all;
bf1.array = 'xyz';
bf1.mic_y = [182 146 36 0]*1e-3;
bf1.mic_x = [0 0 0 0];
bf1.mic_z = [0 0 0 0];

% Despite xyz array this is line, so use the -90 .. +90 degree enum scale for
% angles. Array equals 'line' get this setting automatically.
bf1.angle_enum_mult = 15;
bf1.angle_enum_offs = -90;

% Copy settings to other beam
bf2 = bf1;

% Design beamformer 1 (left)
bf1.steer_az = a1;
bf1.steer_el = 0 * a1;
bf1.input_channel_select        = [0 1 2 3];  % Input four channels
bf1.output_channel_mix          = [1 1 1 1];  % Mix filters to channel 2^0
bf1.output_channel_mix_beam_off = [1 0 0 2];  % Filter 1 to channel 2^0, filter 4 to channel 2^1
bf1.output_stream_mix           = [0 0 0 0];  % Mix filters to stream 0
bf1.num_output_channels = 2;                  % Stereo
bf1.fn = 10;                                  % Figs 10....
bf1 = sof_bf_filenames_helper(bf1);
bf1 = sof_bf_design(bf1);

% Design beamformer 2 (right)
bf2.steer_az = a2;
bf2.steer_el = 0 * a2;
bf2.input_channel_select        = [0 1 2 3];  % Input two channels
bf2.output_channel_mix          = [2 2 2 2];  % Mix filters to channel 2^1
bf2.output_channel_mix_beam_off = [0 0 0 0];  % Filters omitted
bf2.output_stream_mix           = [0 0 0 0];  % Mix filters to stream 0
bf2.num_output_channels = 2;                  % Stereo
bf2.fn = 20;                                  % Figs 20....
bf2 = sof_bf_filenames_helper(bf2);
bf2 = sof_bf_design(bf2);

% Merge two beamformers into single description, set file names
bfm = sof_bf_merge(bf1, bf2);
bfm.sofctl3_fn = fullfile(bfm.sofctl3_path, sofctl3_fn);
bfm.tplg1_fn = fullfile(bfm.tplg1_path, tplg1_fn);
bfm.sofctl4_fn = fullfile(bfm.sofctl4_path, sofctl4_fn);
bfm.tplg2_fn = fullfile(bfm.tplg2_path, tplg2_fn);

% Export files for topology and sof-ctl
bfm.export_note = 'Created with script sof_example_two_beams.m';
bfm.export_howto = 'cd tools/tune/tdfb; matlab -nodisplay -nosplash -nodesktop -r sof_example_two_beams';
sof_bf_export(bfm);

end
