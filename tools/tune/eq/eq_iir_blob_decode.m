function eq = eq_iir_blob_decode(blobfn, resp_n, fs, do_plot)

%% Decode an IIR EQ binary blob
%
% eq = eq_fir_decode_blob(blobfn, resp_n, fs, do_plot)
%
% blobfn  - file name of EQ setup blob
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

%%
% Copyright (c) 2016, Intel Corporation
% All rights reserved.
%
% Redistribution and use in source and binary forms, with or without
% modification, are permitted provided that the following conditions are met:
%   * Redistributions of source code must retain the above copyright
%     notice, this list of conditions and the following disclaimer.
%   * Redistributions in binary form must reproduce the above copyright
%     notice, this list of conditions and the following disclaimer in the
%     documentation and/or other materials provided with the distribution.
%   * Neither the name of the Intel Corporation nor the
%     names of its contributors may be used to endorse or promote products
%     derived from this software without specific prior written permission.
%
% THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
% AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
% IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
% ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
% LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
% CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
% SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
% INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
% CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
% ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
% POSSIBILITY OF SUCH DAMAGE.
%
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
%

if nargin < 4
	do_plot = 0;
end

if nargin < 3
        fs = 48e3;
end

if nargin < 2
        resp_n = 0;
end

%% Defaults
eq.b = [];
eq.a = [];
eq.channels_in_config = 0;
eq.number_of_responses = 0;
eq.assign_response = [];

%% Read binary file
fh = fopen(blobfn, 'rb');
blob = fread(fh, inf, 'uint32');
fclose(fh);

%% Decode
abi = 8;
eq.size = blob(abi + 1);
eq.channels_in_config = blob(abi + 2);
eq.number_of_responses = blob(abi + 3);
reserved1 = blob(abi + 4);
reserved2 = blob(abi + 5);
reserved3 = blob(abi + 6);
reserved4 = blob(abi + 7);
eq.assign_response = blob(abi + 8:abi + 8 + eq.channels_in_config - 1);

fprintf('Blob size = %d\n', eq.size);
fprintf('Channels in config = %d\n', eq.channels_in_config);
fprintf('Number of responses = %d\n', eq.number_of_responses);
fprintf('Assign responses =');
for i=1:length(eq.assign_response)
	fprintf(' %d', eq.assign_response(i));
end
fprintf('\n');

if resp_n > eq.number_of_responses-1;
        error('Request of non-available response');
end

n_blob_header = 7;
n_iir_header = 6;
n_iir_section = 7;

f = logspace(log10(10),log10(fs/2), 500);
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
			if do_plot > 1
				figure;
				h = freqz(b0, a0, f, fs);
				semilogx(f, 20*log10(abs(h)));
				ax = axis; axis([20 20e3 -40 20]);
				grid on;
			end
			eq.b = conv(eq.b, b0);
			eq.a = conv(eq.a, a0);
			k = k + n_iir_section;
                end
        end
        j = j+section_length;
end

if do_plot > 0
	figure;
	h = freqz(eq.b, eq.a, f, fs);
	semilogx(f, 20*log10(abs(h)));
	axis([20 20e3 -40 20]);
	grid on;
	xlabel('Frequency (Hz)');
	ylabel('Amplitude (dB)');
end

end

function y = signedint(x, bits)
y = x;
idx = find(x > 2^(bits-1)-1);
y(idx) = x(idx)-2^bits;
end
