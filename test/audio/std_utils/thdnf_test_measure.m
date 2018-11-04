function test = thdnf_test_measure(test)

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

%% Reference: AES17 6.3.2 THD+N ratio vs frequency
%  http://www.aes.org/publications/standards/

%% Load output file
[x, nx] = load_test_output(test);
if nx == 0
        test.g_db = NaN;
        test.fail = 1;
        return
end

%% Standard low-pass
y0 = stdlpf(x, test.fu, test.fs);

%% Find sync
[d, nt, nt_use, nt_skip] = find_test_signal(y0, test);

%% Measure all test frequencies
ml = zeros(test.nf,test.na);
mn = zeros(test.nf,test.na);
n_notch_settle = round(test.fs*test.nst);
nn = 1;
for m=1:test.na
        for n=1:test.nf
                fprintf('Measuring %.0f Hz ...\n', test.f(n));
                i1 = d+(nn-1)*nt+nt_skip;
                i2 = i1+nt_use-1;
                nn = nn+1;
                y0n = stdnotch(y0(i1:i2), test.f(n), test.fs);
                ml(n,m) = level_dbfs(y0(i1:i2));
                mn(n,m) = level_dbfs(y0n(n_notch_settle:end));
        end
end

test.thdnf = mn - ml;
test.thdnf_high = test.thdnf(:,1);
test.thdnf_low = test.thdnf(:,2);
if max(max(test.thdnf)) > test.thdnf_max
        test.fail = 1;
else
        test.fail = 0;
end

test.fh = figure('visible', test.visible);
semilogx(test.f, test.thdnf(:,1), 'b', test.f, test.thdnf(:,2), 'g--');
grid on;
xlabel('Frequency (Hz)');
ylabel('THD+N (dB)');
legend('-1 dBFS','-20 dBFS');

end
