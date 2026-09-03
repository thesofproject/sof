function eq = sof_eq_fir_blob_decode(blob, resp_n)

%% Decode a FIR EQ binary blob
%
% eq = eq_fir_blob_decode(blob, resp_n, fs, do_plot)
%
% blob    - EQ setup blob 16 bit data vector
% resp_n  - index of response to decode
% fs      - sample rate, optional
% do_plot - set to 1 for frequency response plot, optional
%
% Returned struct fields
% b                  - FIR coefficients
% a                  - Always returns 1 with FIR
% channels_in_config - numbers of channels in blob
% assign_response    - vector of EQ indexes assigned to channels
% size               - length in bytes
%
% To decode a FIR blob, try first
% eq_blob_plot('../../topology/topology1/m4/eq_fir_coef_loudness.m4', 'fir');
% eq_blob_plot('../../ctl/eq_fir_loudness.bin', 'fir');
% eq_blob_plot('../../ctl/eq_fir_loudness.txt', 'fir');

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2016-2020, Intel Corporation. All rights reserved.
%
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

if nargin < 2
	verbose = 1;
	resp_n = [];
else
	verbose = 0;
end

%% Get ABI information
[abi_bytes, nbytes_abi] = sof_get_abi(0);

%% Defaults
eq.b = [];
eq.a = [];
eq.channels_in_config = 0;
eq.number_of_responses = 0;
eq.assign_response = [];

%% Convert to 16 bits
blob16 = zeros(1, length(blob)*2);
for i = 1:length(blob)
	i16 = 2*(i - 1) + 1;
	blob16(i16) = bitand(blob(i), 65535);
	blob16(i16 + 1) = bitshift(blob(i), -16);
end
blob16 = signedint(double(blob16), 16);

%% Decode
abi = nbytes_abi / 2;
eq.size =  blob16(abi + 1) + 65536*blob16(abi + 2);
eq.channels_in_config = blob16(abi + 3);
eq.number_of_responses = blob16(abi + 4);
reserved1 = blob16(abi + 5);
reserved2 = blob16(abi + 6);
reserved3 = blob16(abi + 7);
reserved4 = blob16(abi + 8);
reserved5 = blob16(abi + 9);
reserved6 = blob16(abi + 10);
reserved7 = blob16(abi + 11);
reserved8 = blob16(abi + 12);
eq.assign_response = blob16(abi + 13:abi + 13 + eq.channels_in_config-1);

if verbose
	fprintf('Blob size = %d\n', eq.size);
	fprintf('Channels in config = %d\n', eq.channels_in_config);
	fprintf('Number of responses = %d\n', eq.number_of_responses);
	fprintf('Assign responses =');
	for i=1:length(eq.assign_response)
		fprintf(' %d', eq.assign_response(i));
	end
	fprintf('\n');
end

% Just header request
if isempty(resp_n)
	eq.b = [];
	eq.a = [];
	return
end

% Handle pass-through
if resp_n < 0
	eq.b = 1;
	eq.a = 1;
end

% Normal filter taps retrieve
if resp_n > eq.number_of_responses-1;
        error('Request of non-available response');
end

n_blob_header = 12;
n_fir_header = 10;
b16 = blob16(abi + n_blob_header + eq.channels_in_config + 1:end);
j = 1;
for i=1:eq.number_of_responses
        filter_length = b16(j);
        output_shift = double(b16(j+1));
        if i-1 == resp_n
                bi = b16(j+n_fir_header:j+n_fir_header+filter_length-1);
                eq.b = 2^(-output_shift)*bi/32768;
        end
        j = j+filter_length+n_fir_header;
end
eq.a = 1;

end

function y = signedint(x, bits)
y = x;
idx = find(x > 2^(bits-1)-1);
y(idx) = x(idx)-2^bits;
end
