function example_line_0mm36mm146mm182mm_two_beams()

% Creates beamformer with two beams for device with device with microphones
% at 0, 36, 146, 182mm locations

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2020, Intel Corporation. All rights reserved.
%
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

for fs = [16e3 48e3];
	line_xyz(fs);
end

end

function line_xyz(fs)

% Get defaults
bf1 = bf_defaults();
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
tplg_fn = sprintf('coef_line4_0mm36mm146mm182mm_%s_%dkhz.m4', azstr, fs/1e3);
sofctl_fn = sprintf('coef_line4_0mm36mm146mm182mm_%s_%dkhz.txt', azstr, fs/1e3);
a1 = az;   % Azimuth +az deg
a2 = -az;  % Azimuth -az deg
close all;
bf1.array = 'xyz';
bf1.mic_y = [182 146 36 0]*1e-3;
bf1.mic_x = [0 0 0 0];
bf1.mic_z = [0 0 0 0];
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
