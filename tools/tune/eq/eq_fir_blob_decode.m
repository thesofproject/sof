function eq = eq_fir_blob_decode(blobfn, resp_n, fs, do_plot)

%% Decode a FIR EQ binary blob
%
% eq = eq_fir_blob_decode(blobfn, resp_n, fs, do_plot)
%
% blobfn  - file name of EQ setup blob
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
blob16 = fread(fh, inf, 'int16');
fclose(fh);

%% Decode
abi = 16;
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

n_blob_header = 12;
n_fir_header = 10;
b16 = blob16(abi + n_blob_header + eq.channels_in_config + 1:end);
j = 1;
for i=1:eq.number_of_responses
        filter_length = b16(j);
        output_shift = b16(j+1);
        if i-1 == resp_n
                bi = b16(j+n_fir_header:j+n_fir_header+filter_length-1);
                eq.b = 2^(-output_shift)*double(bi)/32768;
        end
        j = j+filter_length+n_fir_header;
end
eq.a = 1;

if do_plot > 0
	figure;
	f = logspace(log10(10),log10(fs/2), 500);
	h = freqz(eq.b, eq.a, f, fs);
	semilogx(f, 20*log10(abs(h)));
	axis([20 20e3 -40 20]);
	grid on;
	xlabel('Frequency (Hz)');
	ylabel('Amplitude (dB)');
end

end
