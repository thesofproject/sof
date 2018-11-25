function test = aap_test_measure(test)

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

%% Reference: AES17 6.6.6 Attenuation of alias products
%  http://www.aes.org/publications/standards/

%% Load output file
[x, nx] = load_test_output(test);
if nx == 0
        test.fail = 1;
        return
end

%% Find sync
[d, nt, nt_use, nt_skip] = find_test_signal(x, test);

%% Measure
win = hamming(nt_use);
m0 = zeros(test.nf,1);
for n=1:test.nf
        fprintf('Measuring %.0f Hz ...\n', test.f(n));
        i1 = d+(n-1)*nt+nt_skip;
        i2 = i1+nt_use-1;
        m0(n) = level_dbfs(x(i1:i2).*win);
end

%% Calculate levels relative to first 997 Hz frequency,
% remove it from result, sort to ascinding order for plot
m_ref = m0(1);
test.m = m0(2:end)-m0(1);
[test.f, idx] = sort(test.f(2:end));
test.m = test.m(idx);
test.aap = max(test.m); % Worst-case

if test.aap > test.aap_max
        test.fail = 1;
else
        test.fail = 0;
end

test.fh = figure('visible', test.visible);
semilogx(test.f, test.m);
grid on;
xlabel('Frequency (Hz)');
ylabel('Relative level (dB)');
grid on;

end
