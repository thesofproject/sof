function [b, a] = stdnotch_get(fn, fs)

%% [b, a] = stdnotch_get(fn, fs)
%
% Input
% fn - notch frequency
% fs - sample rate
%
% Output
% b - coefficients for filter zeros
% a - coefficients for filter poles
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

target_q = 2.1;
bw = (2*fn/fs)/target_q;
if exist(fullfile('.', 'iirnotch.m'),'file') == 2
        [b, a] = iirnotch(2*fn/fs, bw);
else
        [b, a] = pei_tseng_notch(2*fn/fs, bw);
end
if 0
        %% In AES17 5.2.8 standard notch must have Q 1.2-3.0
        %  Quality factor is ratio of of center frequency to
        %  differenence between -3 dB frequencies.
        figure;
        f = linspace(0.2*fn, min(2*fn,fs/2), 5000);
        h = freqz(b, a, f, fs);
        m = 20*log10(abs(h));
        plot(f, m); grid on;
        idx = find(m < -3);
        f1 = f(idx(1));
        f2 = f(idx(end));
        q = fn/(f2-f1);
        if (q < 1.2) || (q > 3.0)
                fprintf('Required Q value failed, Q = %5.2f\n', q);
        else
                fprintf('Q = %5.2f\n', q);
        end
end
end
