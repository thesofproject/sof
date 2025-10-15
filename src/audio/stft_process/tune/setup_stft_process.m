% setup_stft_process()
%
% Create binary configuration blob for STFT_PROCESS component. The hex data
% is written to tools/topology/topology1/m4/stft_process/stft_process_config.m4

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2025, Intel Corporation.

function setup_stft_process(cfg)


    cfg.tools_path = '../../../../tools/';
    cfg.tplg_path = [cfg.tools_path 'topology/topology2/include/components/stft_process/'];
    cfg.common_path = [cfg.tools_path 'tune/common'];
    cfg.tplg_ver = 2;
    cfg.ipc_ver = 4;
    cfg.channel = 0;
    cfg.sample_frequency = 48000;

    cfg.frame_length = 1024; % 21.3 ms
    cfg.frame_shift = 256; % 5.3 ms
    cfg.window_type = 'hann';
    cfg.tplg_fn = 'hann_1024_256.conf';
    export_stft_process_setup(cfg);

    cfg.frame_length = 1536; % 32 ms
    cfg.frame_shift = 240; % 5 ms
    cfg.window_type = 'hann';
    cfg.tplg_fn = 'hann_1536_240.conf';
    export_stft_process_setup(cfg);

end

function export_stft_process_setup(cfg)

    % Use blob tool from EQ
    addpath(cfg.common_path);

    % Blob size, size plus reserved(8) + current parameters
    nbytes_data = 64;

    % Get ABI information
    [abi_bytes, nbytes_abi] = sof_get_abi(nbytes_data, cfg.ipc_ver);

    % Initialize correct size uint8 array
    nbytes = nbytes_abi + nbytes_data;
    b8 = uint8(zeros(1,nbytes));

    % Insert ABI header
    fprintf(1, 'STFT_PROCESS blob size is %d, ABI header is %d, data is %d\n',nbytes, nbytes_abi, nbytes_data);
    b8(1:nbytes_abi) = abi_bytes;
    j = nbytes_abi + 1;

    % Apply default STFT_PROCESS configuration, first struct header and reserved, then data
    [b8, j] = add_w32b(nbytes_data, b8, j);
    for i = 1:8
	[b8, j] = add_w32b(0, b8, j);
    end

    fft_length = cfg.frame_length;
    fft_hop = cfg.frame_shift;
    [window_idx, window_gain_comp] = get_window(cfg, fft_length, fft_hop);

    v = q_convert(cfg.sample_frequency, 0); [b8, j] = add_w32b(v, b8, j);
    v = q_convert(window_gain_comp, 31);    [b8, j] = add_w32b(v, b8, j);
    v = 0;				    [b8, j] = add_w32b(v, b8, j); % reserved
    v = cfg.channel;			    [b8, j] = add_w16b(v, b8, j);
    v = fft_length;			    [b8, j] = add_w16b(v, b8, j);
    v = fft_hop;			    [b8, j] = add_w16b(v, b8, j);
    v = 0;		                    [b8, j] = add_w16b(v, b8, j); % reserved
    v = 0;				    [b8, j] = add_w32b(v, b8, j); % enum pad
    v = window_idx;		            [b8, j] = add_w32b(v, b8, j); % enum window

    % Export
    switch cfg.tplg_ver
      case 2
	sof_tplg2_write([cfg.tplg_path cfg.tplg_fn], b8, "stft_process_config", ...
			"Exported STFT_PROCESS configuration", ...
			"cd tools/tune/stft_process; octave setup_stft_process.m");
      otherwise
	error("Illegal cfg.tplg_ver, use 2 topology v2.");
    end

    rmpath(cfg.common_path);

end

%% Helper functions

function [idx, iwg] = get_window(cfg, len, hop)
    switch lower(cfg.window_type)
      case 'rectangular'
	win = boxcar(len);
	idx = 0;
      case 'blackman'
	win = blackman(len);
	idx = 1;
      case 'hamming'
	win = hamming(len);
	idx = 2;
      case 'hann'
	win = hann(len);
	idx = 3;
      case 'povey'
	idx = 4;
	n = 0:(len-1);
	win = ((1 - cos(2 * pi * n / len)) / 2).^0.85;
      otherwise
	error('Unknown window type');
        end
        iwg = hop / sum(win.^2);
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
