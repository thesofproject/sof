function sof_example_line_0mm36mm146mm182mm()

% Creates beamformer for device with device with microphones
% at 0, 36, 146, 182mm locations

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2022, Intel Corporation. All rights reserved.
%
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

line_xyz(16e3, 40, -90:15:90);
line_xyz(48e3, 80, -90:15:90);

end

function line_xyz(fs, fir_length, az)

tplg1_fn = sprintf('coef_line4_0mm36mm146mm182mm_%dkhz.m4', fs/1e3);
sofctl3_fn = sprintf('coef_line4_0mm36mm146mm182mm_%dkhz.txt', fs/1e3);
tplg2_fn = sprintf('line4_0mm36mm146mm182mm_%dkhz.conf', fs/1e3);
sofctl4_fn = sprintf('line4_0mm36mm146mm182mm_%dkhz.txt', fs/1e3);

% Get defaults
close all;
bf = sof_bf_defaults();
bf.fs = fs;
bf.beta = 5;

% Setup array
bf.array = 'xyz';
bf.mic_y = [182 146 36 0]*1e-3;
bf.mic_x = [0 0 0 0];
bf.mic_z = [0 0 0 0];

% Despite xyz array this is line, so use the -90 .. +90 degree enum scale for
% angles. Array equals 'line' get this setting automatically.
bf.angle_enum_mult = 15;
bf.angle_enum_offs = -90;

% Design beamformer
bf.fir_length = fir_length;
bf.steer_az = az;
bf.steer_el = 0 * az;
bf.input_channel_select        = [0 1 2 3];  % Input four channels
bf.output_channel_mix          = [3 3 3 3];  % Mix filters to channel 2^0 and 2^1
bf.output_channel_mix_beam_off = [1 0 0 2];  % Filter 1 to channel 2^0, filter 4 to channel 2^1
bf.output_stream_mix           = [0 0 0 0];  % Mix filters to stream 0
bf.num_output_channels = 2;                  % Stereo

%bf.sofctl_fn = fullfile(bf.sofctl_path, sofctl_fn);
%bf.tplg_fn = fullfile(bf.tplg_path, tplg_fn);
bf = sof_bf_filenames_helper(bf, 'line4_0mm36mm146mm182mm');
bf = sof_bf_design(bf);

% Export files for topology and sof-ctl
bf.export_note = 'Created with script example_line_0mm36mm146mm182mm.m';
bf.export_howto = 'cd tools/tune/tdfb; octave --no-window-system example_line_0mm36mm146mm182mm.m';
sof_bf_export(bf);

end
