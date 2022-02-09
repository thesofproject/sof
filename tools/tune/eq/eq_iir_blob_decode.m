function eq = eq_iir_blob_decode(ublob, resp_n)

%% Decode an IIR EQ binary blob
%
% eq = eq_fir_decode_blob(blobfn, resp_n)
%
% blob    - EQ setup blob 32 bit data vector
% resp_n  - index of response to decode
% fs      - sample rate, optional
% do_plot - set to 1 for frequency response plot, optional
%
% Returned struct fields
% b                  - numerator coefficients
% a                  - denominator coefficients
% channels_in_config - numbers of channels in blob
% assign response    - vector of EQ indexes assigned to channels
%
% To decode a IIR blob, try iirst
% eq_blob_plot('../../topology/topology1/m4/eq_iir_coef_loudness.m4', 'iir');
% eq_blob_plot('../../ctl/eq_iir_loudness.bin', 'iir');
% eq_blob_plot('../../ctl/eq_iir_loudness.txt', 'iir');

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
[abi_bytes, nbytes_abi] = eq_get_abi(0);

%% Defaults
eq.b = [];
eq.a = [];
eq.channels_in_config = 0;
eq.number_of_responses = 0;
eq.assign_response = [];

%% Decode
blob = double(ublob);
abi = nbytes_abi / 4;
eq.size = blob(abi + 1);
eq.channels_in_config = blob(abi + 2);
eq.number_of_responses = blob(abi + 3);
reserved1 = blob(abi + 4);
reserved2 = blob(abi + 5);
reserved3 = blob(abi + 6);
reserved4 = blob(abi + 7);
eq.assign_response = signedint(blob(abi + 8:abi + 8 + eq.channels_in_config - 1), 32);

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

n_blob_header = 7;
n_iir_header = 6;
n_iir_section = 7;

j = abi + n_blob_header + eq.channels_in_config + 1;
eq.b = 1.0;
eq.a = 1.0;
for i=1:eq.number_of_responses
	num_sections = blob(j);
        section_length = num_sections * n_iir_section + n_iir_header;
        if i-1 == resp_n
                k = j + n_iir_header;
                for j=1:num_sections
                        ai = signedint(blob(k+1:-1:k),32)';
                        bi = signedint(blob(k+4:-1:k+2),32)';
                        shifti = signedint(blob(k+5),32);
                        gaini = signedint(blob(k+6),16);
                        b0 = bi/2^30;
                        a0 = [1 -(ai/2^30)];
                        gain = gaini/2^14;
                        b0 = b0 * 2^(-shifti) * gain;
			eq.b = conv(eq.b, b0);
			eq.a = conv(eq.a, a0);
			k = k + n_iir_section;
                end
        end
        j = j+section_length;
end

end

function y = signedint(x, bits)
y = x;
idx = find(x > 2^(bits-1)-1);
y(idx) = x(idx)-2^bits;
end
