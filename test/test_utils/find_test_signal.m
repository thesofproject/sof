function [d, nt, nt_use, nt_skip] = find_test_signal(x, test)

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

%% Find start marker
fprintf('Finding test start marker...\n');
s = sync_chirp(test.fs, 'up');
nx = length(x);
n_seek = round(test.fs*(test.idle_t + test.mark_t));
n = min(round(test.fs*test.sm), n_seek);
y = x(1:n);
[r, lags] = xcorr(y, s);
[r_max, idx] = max(r);
d_start = lags(idx);

%% Find end marker
fprintf('Finding test end marker...\n');
s = sync_chirp(test.fs, 'down');
n_seek = round(test.fs*(2*test.idle_t + test.mark_t));
n = min(round(test.fs*test.em),n_seek);
y = x(end-n+1:end);
[r, lags] = xcorr(y, s);
[r_max, idx] = max(r);
d_end = nx-n+lags(idx);

%% Check correct length of signal
len = d_end-d_start;
len_s = len/test.fs;
ref_s = test.mark_t+test.nf*test.na*test.tl;
if abs(len_s-ref_s) > test.mt
        len_s
        ref_s
        error('Start and end markers were not found. Test play or capture quality may be poor');
end

%% Delay to first tone, length of tone in samples
d = d_start + round(test.mark_t*test.fs);
if (d < 0)
	error('Invalid negative delay seen. Test play or capture quality may be poor');
end
nt = round(test.tl*test.fs);
nt_use = nt -round(test.is*test.fs) -round(test.ie*test.fs);
if nt_use < 0
        error('Test signal length parameters must be wrong!');
end
nt_skip = round(test.is*test.fs);

end
