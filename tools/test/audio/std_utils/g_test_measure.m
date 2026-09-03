function test = g_test_measure(test)

% SPDX-License-Identifier: BSD-3-Clause
% Copyright(c) 2017 Intel Corporation. All rights reserved.
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

%% Reference: AES17 6.2.2 Gain
%  http://www.aes.org/publications/standards/

default_result = NaN * ones(1,length(test.ch));
test.g_db = default_result;

%% Load output file
[x, nx] = load_test_output(test);

if nx == 0
	test.fail = 1;
	return
end

%% Standard low-pass
y0 = stdlpf(x, test.fu, test.fs);

%% Find sync
[d, nt, nt_use, nt_skip] = find_test_signal(y0, test);
if isempty(d)
	test.fail = 1;
	return
end

%% Trim sample by removing first 1s to let the notch to apply
i1 = d+nt_skip;
i2 = i1+nt_use-1;
y = y0(i1:i2, :);

%% Gain, SNR
level_in = test.a_db;
level_out = level_dbfs(y);
test.g_db = level_out - level_in + test.att_rec_db;
for i = 1:length(test.g_db)
	fprintf('Gain = %6.3f dB (expect %6.3f dB)\n', test.g_db(i), test.g_db_expect(i));
end

%% Check pass/fail
test.fail = 0;
delta_abs = abs(test.g_db_expect - test.g_db);
idx = find(delta_abs >  test.g_db_tol);
for i = 1:length(idx)
	fprintf('Failed ch%d gain %f dB (max %f dB)\n', test.ch(i), test.g_db(i), test.g_db_tol);
	test.fail = 1;
end

end
