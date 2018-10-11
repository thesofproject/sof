function y = stdweight(x, fs)

%% y = stdweight(x, fs)
%
% Input
% x  - input signal
% fs - sample rate
%
% Output
% y - filtered signal
%

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

%% Frequency grid for FIR design
f = linspace(0, fs/2, 6000);

%% From https://en.wikipedia.org/wiki/ITU-R_468_noise_weighting
h1 = -4.737338981378384e-24*f.^6 +2.043828333606125e-15*f.^4 -1.363894795463638e-7*f.^2 +1;
h2 =  1.306612257412824e-19*f.^5 -2.118150887518656e-11*f.^3 +5.559488023498642e-4*f;
RITU = 1.246332637532143e-4*f./sqrt(h1.^2+h2.^2);
ITU = 18.2+20*log10(RITU)-5.63; % AES17 CCIR-RMS
fc = [ 31.5    63   100   200   400   800  1000  2000  3150  4000  5000  6300  7100  8000  9000 10000 12500 14000 16000 20000 31500];
ga = [-35.5 -29.5 -25.4 -19.4 -13.4  -7.5  -5.6     0   3.4   4.9   6.1   6.6   6.4   5.8   4.5   2.5  -5.6 -10.9 -17.3 -27.8 -48.3];
tu = [  2.0   1.4   1.0  0.85  0.70  0.55  0.50  0.50  0.50  0.50  0.50  0.01  0.20  0.40  0.60  0.80   1.2   1.4   1.6   2.0   2.8];
tl = [ -2.0  -1.4  -1.0 -0.85 -0.70 -0.55 -0.50 -0.50 -0.50 -0.50 -0.50 -0.01 -0.20 -0.40 -0.60 -0.80  -1.2  -1.4  -1.6  -2.0   NaN];

%% Fine tune the 6300Hz, 6.6 dB point
if fs > 2*6300
        idx = find(f > 6300, 1, 'first');
        ITU = ITU + 6.6-ITU(idx);
else
        idx = find(f > 3150, 1, 'first');
        ITU = ITU + 3.4-ITU(idx);
end

m_fir = 10.^(ITU/20);
f_fir = 2*f/fs;
n_fir = 4000; % Sufficient up to 192 kHz
if (fs < 96001)
        n_fir = 2000;
end
if (fs < 48001)
        n_fir = 1000;
end
if (fs < 16001)
        n_fir = 400;
end
bz = fir2(n_fir, f_fir, m_fir);
y = filter(bz, 1, x);
if 0
        h = freqz(bz, 1, f, fs);
        my_w_db = 20*log10(abs(h));
        figure;
        semilogx(f,ITU,fc,ga,'x',fc,ga+tu,fc,ga+tl, f, my_w_db );
        grid on; xlabel('Hz'); ylabel('dB');
end
end
