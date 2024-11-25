function sof_bf_line4_two_beams(fs, d, a1, a2, fn, prm)

% sof_bf_line4_two_beams(fs, d, a1, a2, fn, prm)
% Input
%   fs - sample rate
%   d - microphones distance in meters
%   a1 - steer angle beam 1
%   a2 - steer angle beam 2
%   fn - struct with exported blob files names
%   prm
%      .add_beam_beam_off - controls addition of beam off definition to blob
%      .type - Use 'SDB' or 'DSB'
%      .export_note - comment about build generally
%      .export_howto - detailed build instruction
%

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2020-2024, Intel Corporation. All rights reserved.
%
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

% Get defaults
bf1 = sof_bf_defaults();
bf1.fs = fs;

% Setup array
bf1.array='line';          % Calculate xyz coordinates for line
bf1.mic_n = 4;
bf1.mic_d = d;
bf1.beam_off_defined = prm.add_beam_off;
bf1.type = prm.type;

% Copy settings for bf2
bf2 = bf1;

% Design beamformer 1 (left)
bf1.steer_az = a1;
bf1.steer_el = 0 * a1;
bf1.input_channel_select        = [0 1 2 3];  % Input four channels
bf1.output_channel_mix          = [1 1 1 1];  % Mix filters to channel 2^0
bf1.output_channel_mix_beam_off = [1 0 0 2];  % Filter 1 to channel 2^0, filter 4 to channel 2^1
bf1.output_stream_mix           = [0 0 0 0];  % Mix filters to stream 0
bf1.num_output_channels = 2;
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
bf2.num_output_channels = 2;
bf2.fn = 20;                                      % Figs 20....
bf2 = sof_bf_filenames_helper(bf2);
bf2 = sof_bf_design(bf2);

% Merge two beamformers into single description, set file names
bfm = sof_bf_merge(bf1, bf2);
bfm.sofctl3_fn = fullfile(bfm.sofctl3_path, fn.sofctl3_fn);
bfm.tplg1_fn = fullfile(bfm.tplg1_path, fn.tplg1_fn);
bfm.sofctl4_fn = fullfile(bfm.sofctl4_path, fn.sofctl4_fn);
bfm.tplg2_fn = fullfile(bfm.tplg2_path, fn.tplg2_fn);

% Export files for topology and sof-ctl
bfm.export_note = prm.export_note;
bfm.export_howto = prm.export_howto;
sof_bf_export(bfm);

end
