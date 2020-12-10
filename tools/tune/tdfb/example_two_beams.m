function example_two_beams()

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

az = [0 30 90];
azstr = az_to_string(az);
for fs = [16e3 48e3]
	%% Close all plots to avoid issues with large number of windows
	close all;

	%% 2 mic 50 mm array
	tplg_fn = sprintf('coef_line2_50mm_pm%sdeg_%dkhz.m4', azstr, fs/1e3);
	sofctl_fn = sprintf('coef_line2_50mm_pm%sdeg_%dkhz.txt', azstr, fs/1e3);
	d = 50e-3;  % 50 mm spacing
	a1 = az;   % Azimuth +az deg
	a2 = -az;  % Azimuth -az deg
	line2_two_beams(fs, d, a1, a2, tplg_fn, sofctl_fn);

	%% 2 mic 68 mm array
	tplg_fn = sprintf('coef_line2_68mm_pm%sdeg_%dkhz.m4', azstr, fs/1e3);
	sofctl_fn = sprintf('coef_line2_68mm_pm%sdeg_%dkhz.txt', azstr, fs/1e3);
	d = 68e-3;  % 68 mm spacing
	a1 = az;   % Azimuth +az deg
	a2 = -az;  % Azimuth -az deg
	line2_two_beams(fs, d, a1, a2, tplg_fn, sofctl_fn);

	%% 4 mic 28 mm spaced array
	tplg_fn = sprintf('coef_line4_28mm_pm%sdeg_%dkhz.m4', azstr, fs/1e3);
	sofctl_fn = sprintf('coef_line4_28mm_pm%sdeg_%dkhz.txt', azstr, fs/1e3);
	d = 28e-3;  % 28 mm spacing
	a1 = az;   % Azimuth +az deg
	a2 = -az;  % Azimuth -az deg
	line4_two_beams(fs, d, a1, a2, tplg_fn, sofctl_fn);

	%% 4 mic 78 mm spaced array
	tplg_fn = sprintf('coef_line4_78mm_pm%sdeg_%dkhz.m4', azstr, fs/1e3);
	sofctl_fn = sprintf('coef_line4_78mm_pm%sdeg_%dkhz.txt', azstr, fs/1e3);
	d = 78e-3;  % 78 mm spacing
	a1 = az;   % Azimuth +az deg
	a2 = -az;  % Azimuth -az deg
	line4_two_beams(fs, d, a1, a2, tplg_fn, sofctl_fn);
end

%% Export blob with just +/- 90 deg beams for testbench beampattern check
close all;
az = [90];
azstr = az_to_string(az);
for fs = [16e3 48e3]
	%% 2 mic 50 mm array
	tplg_fn = sprintf('coef_line2_50mm_pm%sdeg_%dkhz.m4', azstr, fs/1e3);
	sofctl_fn = sprintf('coef_line2_50mm_pm%sdeg_%dkhz.txt', azstr, fs/1e3);
	d = 50e-3;  % 50 mm spacing
	a1 = az;   % Azimuth +az deg
	a2 = -az;  % Azimuth -az deg
	line2_two_beams(fs, d, a1, a2, tplg_fn, sofctl_fn);

	%% 4 mic 28 mm spaced array
	tplg_fn = sprintf('coef_line4_28mm_pm%sdeg_%dkhz.m4', azstr, fs/1e3);
	sofctl_fn = sprintf('coef_line4_28mm_pm%sdeg_%dkhz.txt', azstr, fs/1e3);
	d = 28e-3;  % 28 mm spacing
	a1 = az;   % Azimuth +az deg
	a2 = -az;  % Azimuth -az deg
	line4_two_beams(fs, d, a1, a2, tplg_fn, sofctl_fn);
end

end

function s = az_to_string(az)
	s = sprintf('%d', az(1));
	for n = 2:length(az)
		s = sprintf('%s_%d', s, az(n));
	end
end

function line2_two_beams(fs, d, a1, a2, tplg_fn, sofctl_fn);

% Get defaults
bf1 = bf_defaults();
bf1.fs = fs;

% Setup array
bf1.array='line';          % Calculate xyz coordinates for line
bf1.mic_n = 2;
bf1.mic_d = d;

% Copy settings for bf2
bf2 = bf1;

% Design beamformer 1 (left)
bf1.steer_az = a1;
bf1.steer_el = 0 * a1;
bf1.input_channel_select        = [0 1];  % Input two channels
bf1.output_channel_mix          = [1 1];  % Mix both filters to channel 2^0
bf1.output_channel_mix_beam_off = [1 2];  % Filter 1 to channel 2^0, etc.
bf1.output_stream_mix           = [0 0];  % Mix both filters to stream 0
bf1.num_output_channels = 2;
bf1.fn = 10;                              % Figs 10....
bf1 = bf_filenames_helper(bf1);
bf1 = bf_design(bf1);

% Design beamformer 2 (right)
bf2.steer_az = a2;
bf2.steer_el = 0 * a2;
bf2.input_channel_select        = [0 1];  % Input two channels
bf2.output_channel_mix          = [2 2];  % Mix both filters to channel 2^1
bf2.output_channel_mix_beam_off = [0 0];  % Filters omitted
bf2.output_stream_mix           = [0 0];  % Mix both filters to stream 0
bf2.num_output_channels = 2;
bf2.fn = 20;                              % Figs 20....
bf2 = bf_filenames_helper(bf2);
bf2 = bf_design(bf2);

% Merge two beamformers into single description, set file names
bfm = bf_merge(bf1, bf2);
bfm.sofctl_fn = fullfile(bfm.sofctl_path, sofctl_fn);
bfm.tplg_fn = fullfile(bfm.tplg_path, tplg_fn);

% Export files for topology and sof-ctl
bf_export(bfm);

end

function line4_two_beams(fs, d, a1, a2, tplg_fn, sofctl_fn);

% Get defaults
bf1 = bf_defaults();
bf1.fs = fs;

% Setup array
bf1.array='line';          % Calculate xyz coordinates for line
bf1.mic_n = 4;
bf1.mic_d = d;

% Copy settings for bf2
bf2 = bf1;

% Design beamformer 1 (left)
bf1.steer_az = a1;
bf1.steer_el = 0 * a1;
bf1.input_channel_select        = [0 1 2 3];  % Input four channels
bf1.output_channel_mix          = [5 5 5 5];  % Mix filters to channel 2^0
bf1.output_channel_mix_beam_off = [1 2 4 8];  % Filter 1 to channel 2^0 etc.
bf1.output_stream_mix           = [0 0 0 0];  % Mix filters to stream 0
bf1.num_output_channels = 4;
bf1.fn = 10;                                  % Figs 10....
bf1 = bf_filenames_helper(bf1);
bf1 = bf_design(bf1);

% Design beamformer 2 (right)
bf2.steer_az = a2;
bf2.steer_el = 0 * a2;
bf2.input_channel_select        = [ 0  1  2  3];  % Input two channels
bf2.output_channel_mix          = [10 10 10 10];  % Mix filters to channel 2^1 and 2^3
bf2.output_channel_mix_beam_off = [ 0  0  0  0];  % Filters omitted
bf2.output_stream_mix           = [ 0  0  0  0];  % Mix filters to stream 0
bf2.num_output_channels = 4;
bf2.fn = 20;                                      % Figs 20....
bf2 = bf_filenames_helper(bf2);
bf2 = bf_design(bf2);

% Merge two beamformers into single description, set file names
bfm = bf_merge(bf1, bf2);
bfm.sofctl_fn = fullfile(bfm.sofctl_path, sofctl_fn);
bfm.tplg_fn = fullfile(bfm.tplg_path, tplg_fn);

% Export files for topology and sof-ctl
bf_export(bfm);

end
