function sof_example_pass_config()

% example_pass_config()
%
% Creates a number for topologies a special configuration blob
% that instantiates two min. length filters and configures
% each channel to pass.

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2020, Intel Corporation. All rights reserved.
%
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

% Setup for two channels
bf = sof_bf_defaults();
bf.input_channel_select = [0 1]; % Input two channels
bf.output_channel_mix   = [1 2]; % Filter1 -> ch0, filter2 -> ch1
bf.output_stream_mix    = [0 0]; % Mix both filters to stream 0
bf.num_output_channels  = 2;     % Two channels
bf.num_output_streams   = 1;     % One sink stream
bf.beam_off_defined     = 0;     % No need for separate bypass definition
bf.num_angles           = 1;     % Need at least one beam defined even
bf.steer_az             = 0;     %   if no processing happens, claim it's
bf.steer_el             = 0;     %   angle (0, 0).
bf.num_filters          = 2;     % Two filters

% Minimal manual design fields for successful export
bf.w = [1 0 0 0; 1 0 0 0]'; % Two FIR filters with first tap set to one

% Files
bf.export_note = 'Exported with script sof_example_pass_config.m';
bf.export_howto = 'cd tools/tune/tdfb; octave --no-window-system sof_example_pass_config.m';
bf.sofctl3_fn = fullfile(bf.sofctl3_path, 'coef_line2_pass.txt');
bf.tplg1_fn = fullfile(bf.tplg1_path, 'coef_line2_pass.m4');
bf.sofctl4_fn = fullfile(bf.sofctl4_path, 'line2_pass.txt');
bf.ucmbin3_fn = fullfile(bf.sofctl3_path, 'line2_pass.bin');
bf.ucmbin4_fn = fullfile(bf.sofctl4_path, 'line2_pass.bin');
bf.tplg2_fn = fullfile(bf.tplg2_path, 'line2_pass.conf');
sof_bf_export(bf);

% Setup for four channels
bf.input_channel_select = [0 1 2 3]; % Input four channels
bf.output_channel_mix   = [1 2 4 8]; % Filter1 -> ch0, filter2 -> ch1, ...
bf.output_stream_mix    = [0 0 0 0]; % Mix both filters to stream 0
bf.num_output_channels  = 4;         % Four channels
bf.num_output_streams   = 1;         % One sink stream

% Minimal manual design fields for successful export
bf.num_filters = 4;
bf.w = [1 0 0 0; 1 0 0 0; 1 0 0 0; 1 0 0 0]'; % Four FIR filters with first tap set to one

% Files
bf.sofctl3_fn = fullfile(bf.sofctl3_path, 'coef_line4_pass.txt');
bf.tplg1_fn = fullfile(bf.tplg1_path, 'coef_line4_pass.m4');
bf.sofctl4_fn = fullfile(bf.sofctl4_path, 'line4_pass.txt');
bf.ucmbin3_fn = fullfile(bf.sofctl3_path, 'line4_pass.bin');
bf.ucmbin4_fn = fullfile(bf.sofctl4_path, 'line4_pass.bin');
bf.tplg2_fn = fullfile(bf.tplg2_path, 'line4_pass.conf');
sof_bf_export(bf);


% Setup for four channels to two channels passthrough

bf.input_channel_select = [0 3]; % Input two channels, leftmost, rightmost mic of 4
bf.output_channel_mix   = [1 2]; % Filter1 -> ch0, filter2 -> ch1
bf.output_stream_mix    = [0 0]; % Mix both filters to stream 0
bf.num_output_channels  = 2;     % Two channels
bf.num_output_streams   = 1;     % One sink stream

% Minimal manual design fields for successful export
bf.num_filters = 2;
bf.w = [1 0 0 0; 1 0 0 0]'; % Two FIR filters with first tap set to one

% Files
bf.sofctl3_fn = fullfile(bf.sofctl3_path, 'coef_line4to2_pass.txt');
bf.tplg1_fn = fullfile(bf.tplg1_path, 'coef_line4to2_pass.m4');
bf.sofctl4_fn = fullfile(bf.sofctl4_path, 'line4to2_pass.txt');
bf.ucmbin3_fn = fullfile(bf.sofctl3_path, 'line4to2_pass.bin');
bf.ucmbin4_fn = fullfile(bf.sofctl4_path, 'line4to2_pass.bin');
bf.tplg2_fn = fullfile(bf.tplg2_path, 'line4to2_pass.conf');
sof_bf_export(bf);

end
