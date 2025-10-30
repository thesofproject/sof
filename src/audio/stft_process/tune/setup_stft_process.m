% setup_stft_process(cfg)
%
% Input
%   cfg - optional STFT_PROCESS configuration parameters struct, see
%         below from code
%
% Create binary configuration blob for STFT_PROCESS component. The hex data
% is written to tools/topology/topology1/m4/stft_process/stft_process_config.m4

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2025, Intel Corporation.

function setup_stft_process(cfg)

if nargin < 1
	cfg.blackman_coef = 0.42;
	cfg.cepstral_lifter = 22.0;
	cfg.channel = 0; % -1 expect mono, 0 left, 1 right ...
	cfg.dither = 0.0; % no support
	cfg.energy_floor = 1.0;
	%cfg.frame_length = 256*1e3/48000;
	%cfg.frame_length = 512*1e3/48000;
	cfg.frame_length = 1024*1e3/48000;
	cfg.frame_shift = cfg.frame_length / 4;
	cfg.high_freq = 0.0; % 0 is Nyquist
	cfg.htk_compat = false; % no support
	cfg.low_freq = 20.0; % Hz
	cfg.num_ceps = 13;
	cfg.min_duration = 0.0; % no support
	cfg.norm = 'none'; % Use none or slaney
	cfg.num_mel_bins = 23;
	cfg.preemphasis_coefficient = 0; % disable
	cfg.raw_energy = true;
	cfg.remove_dc_offset = true;
	cfg.round_to_power_of_two = true; % must be true
	cfg.sample_frequency = 48000;
	cfg.snip_edges = true; % must be true
	cfg.subtract_mean = false; % must be false
	cfg.use_energy = false;
	cfg.vtln_high = -500.0; % no support
	cfg.vtln_low = 100.0; % no support
	cfg.vtln_warp = 1.0; % must be 1.0 (vocal tract length normalization)
	%cfg.window_type = 'rectangular';
	%cfg.window_type = 'hamming';
	cfg.window_type = 'hann';
	cfg.mel_log = 'log'; % Set to 'db' for librosa, set to 'log10' for matlab
	cfg.pmin = 5e-10; % Set to 1e-10 for librosa
	cfg.top_db = 200; % Set to 80 for librosa
end

cfg.tplg_fn = '../../../../tools/topology/topology2/include/components/stft_process/default.conf';
cfg.tplg_ver = 2;
cfg.ipc_ver = 4;
export_stft_process_setup(cfg);

end

function export_stft_process_setup(cfg)

%% Use blob tool from EQ
addpath('../../../../tools/tune/common');

%% Blob size, size plus reserved(8) + current parameters
nbytes_data = 104;

%% Little endian
sh32 = [0 -8 -16 -24];
sh16 = [0 -8];

%% Get ABI information
[abi_bytes, nbytes_abi] = sof_get_abi(nbytes_data, cfg.ipc_ver);

%% Initialize correct size uint8 array
nbytes = nbytes_abi + nbytes_data;
b8 = uint8(zeros(1,nbytes));

%% Insert ABI header
fprintf(1, 'STFT_PROCESS blob size is %d, ABI header is %d, data is %d\n',nbytes, nbytes_abi, nbytes_data);
b8(1:nbytes_abi) = abi_bytes;
j = nbytes_abi + 1;

%% Apply default STFT_PROCESS configuration, first struct header and reserved, then data
[b8, j] = add_w32b(nbytes_data, b8, j);
for i = 1:8
	[b8, j] = add_w32b(0, b8, j);
end

v = q_convert(cfg.sample_frequency, 0);          [b8, j] = add_w32b(v, b8, j);
v = q_convert(cfg.pmin, 31);                     [b8, j] = add_w32b(v, b8, j);
v = 0;                                           [b8, j] = add_w32b(v, b8, j); % enum mel_log
v = 0;                                           [b8, j] = add_w32b(v, b8, j); % enum norm
v = 0;                                           [b8, j] = add_w32b(v, b8, j); % enum pad
v = get_window(cfg);                             [b8, j] = add_w32b(v, b8, j); % enum window
v = 1;                                           [b8, j] = add_w32b(v, b8, j); % enum dct type
v = q_convert(cfg.blackman_coef, 15);            [b8, j] = add_w16b(v, b8, j);
v = q_convert(cfg.cepstral_lifter, 9);           [b8, j] = add_w16b(v, b8, j);
v = cfg.channel;                                 [b8, j] = add_w16b(v, b8, j);
v = cfg.dither;                                  [b8, j] = add_w16b(v, b8, j);
v = round(cfg.frame_length/1000 * cfg.sample_frequency); [b8, j] = add_w16b(v, b8, j);
v = round(cfg.frame_shift/1000 * cfg.sample_frequency); [b8, j] = add_w16b(v, b8, j);
v = q_convert(cfg.high_freq, 0);                 [b8, j] = add_w16b(v, b8, j);
v = q_convert(cfg.low_freq, 0);                  [b8, j] = add_w16b(v, b8, j);
v = cfg.num_ceps;                                [b8, j] = add_w16b(v, b8, j);
v = cfg.num_mel_bins;                            [b8, j] = add_w16b(v, b8, j);
v = q_convert(cfg.preemphasis_coefficient, 15);  [b8, j] = add_w16b(v, b8, j);
v = q_convert(cfg.top_db, 7);                    [b8, j] = add_w16b(v, b8, j);
v = 0;                                           [b8, j] = add_w16b(v, b8, j); % vtln_high Qx.y TBD
v = 0;                                           [b8, j] = add_w16b(v, b8, j); % vtln_low Qx.y TBD
v = 0;                                           [b8, j] = add_w16b(v, b8, j); % vtln_warp Qx.y TBD
v = cfg.htk_compat;                              [b8, j] = add_w8b(v, b8, j); % bool
v = cfg.raw_energy;                              [b8, j] = add_w8b(v, b8, j); % bool
v = cfg.remove_dc_offset;                        [b8, j] = add_w8b(v, b8, j); % bool
v = cfg.round_to_power_of_two;                   [b8, j] = add_w8b(v, b8, j); % bool
v = cfg.snip_edges;                              [b8, j] = add_w8b(v, b8, j); % bool
v = cfg.subtract_mean;                           [b8, j] = add_w8b(v, b8, j); % bool
v = cfg.use_energy;                              [b8, j] = add_w8b(v, b8, j); % bool

%% Export
switch cfg.tplg_ver
       case 2
	       sof_tplg2_write(cfg.tplg_fn, b8, "stft_process_config", ...
			       "Exported STFT_PROCESS configuration", ...
			       "cd tools/tune/stft_process; octave setup_stft_process.m");
       otherwise
	       error("Illegal cfg.tplg_ver, use 2 topology v2.");
end

rmpath('../../../../tools/tune/common');

end

%% Helper functions

function n = get_window(cfg)
	switch lower(cfg.window_type)
		case 'rectangular'
			n = 0;
		case 'blackman'
			n = 1;
		case 'hamming'
			n = 2;
		case 'hann'
			n = 3;
		case 'povey'
			n = 4;
		otherwise
			error('Unknown window type');
	end
end

function bytes = w8b(word)
bytes = uint8(zeros(1,1));
bytes(1) = bitand(word, 255);
end

function bytes = w16b(word)
sh = [0 -8];
bytes = uint8(zeros(1,2));
bytes(1) = bitand(bitshift(word, sh(1)), 255);
bytes(2) = bitand(bitshift(word, sh(2)), 255);
end

function bytes = w32b(word)
sh = [0 -8 -16 -24];
bytes = uint8(zeros(1,4));
bytes(1) = bitand(bitshift(word, sh(1)), 255);
bytes(2) = bitand(bitshift(word, sh(2)), 255);
bytes(3) = bitand(bitshift(word, sh(3)), 255);
bytes(4) = bitand(bitshift(word, sh(4)), 255);
end

function n = q_convert(val, q)
n = round(val * 2^q);
end

function [blob8, j] = add_w8b(v, blob8, j)
if j > length(blob8)
	error('Blob size is not sufficient');
end
blob8(j) = w8b(v);
j = j + 1;
end

function [blob8, j] = add_w16b(v, blob8, j)
if j + 1 > length(blob8)
	error('Blob size is not sufficient');
end
if v < 0
	v = 2^16 + v;
end
blob8(j : j + 1) = w16b(v);
j = j + 2;
end

function [blob8, j] = add_w32b(v, blob8, j)
if j + 3 > length(blob8)
	error('Blob size is not sufficient');
end
if v < 0
	v = 2^32 + v;
end
blob8(j : j + 3) = w32b(v);
j = j + 4;
end
