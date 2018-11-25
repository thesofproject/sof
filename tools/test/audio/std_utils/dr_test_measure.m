function test = dr_test_measure(test)

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

%% Load output file
[x, nx] = load_test_output(test);
if nx == 0
        test.dr_db = NaN;
        test.fail = 1;
        return
end

%% Find sync
[d, nt, nt_use, nt_skip] = find_test_signal(x, test);

%% Notch filter
y0n = stdnotch(x, test.f, test.fs);

%% CCIR-RMS weighting filter
y0w = stdweight(y0n, test.fs);

%% Trim sample by removing first 1s to let the notch to apply
i1 = d+nt_skip;
i2 = i1+nt_use-1;
y = x(i1:i2);
y_n = y0w(i1:i2);

%% Gain, SNR
level_in = test.a_db;
level_out = level_dbfs(y);
level_final = level_dbfs(y_n);
test.dr_db = level_out-level_final-test.a_db;
fprintf('DR = %5.1f dB\n', test.dr_db);
if 0
        plot(y);
        hold on
        plot(y_n,'g');
        hold off
end

%% Check pass/fail
test.fail = 0;
if test.dr_db < test.dr_db_min
        fprintf('Failed DR %f dB (min %f dB)\n', test.dr_db, test.dr_db_min);
        test.fail = 1;
end

end
