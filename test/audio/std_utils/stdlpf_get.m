function b = stdlpf_get(fu, fs)

%% b = stdlpf_get(fu, fs)
%
% Standard loww-pass filter
%
% Input
% fu - upper band-edge frequency
% fs - sample rate
%
% Output
% b - coefficients for FIR filter
%
% Reference AES17 5.2.5

%%
% Copyright (c) 2017, Intel Corporation
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

%% Design filter
rp = 0.001; % Since this may need to be in frequency response measurement the ripple must be very small
rs = -65; % Some margin vs. requirement
a = [1 0];
% Note: The fc-fu for digital low-pass is not feasible. Use of fu*1.2 instead
% results to example 24 kHz for 20 kHz upper band-edge. This could be a mistake
% in specification.
fc = min(fu*1.2, 0.99*fs/2);
f = [fu fc];
dev = [ (10^(rp/20)-1)/(10^(rp/20)+1) 10^(rs/20) ];
[n, wn, beta, ftype] = kaiserord(f, a, dev, fs);
b = fir1(n, wn, ftype, kaiser(n+1,beta));

if 0
        f = linspace(0, fs/2, 1000);
        h = freqz(b,1, f, fs);
        figure
        plot(f, 20*log10(abs(h)))
        hold on;
        plot( [fc fs/2], [-60 -60], 'r', [0 fu], [-0.1 -0.1], 'r', [0 fu], [0.1 0.1], 'r');
        hold off;
        grid on;
end

end
