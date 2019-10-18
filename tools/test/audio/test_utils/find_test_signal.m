function [d, nt, nt_use, nt_skip] = find_test_signal(x0, test)

%% [d, nt, nt_use, nt_skip] = find_test_signal(x, test)
%
% Inputs
% x    - input signal
% test - test definitions
%
% Outputs
% d       - delay to fist test tone start
% nt      - number of samples in test tone
% nt_use  - number of samples to use
% nt_skip - number of samples to skip
%

% SPDX-License-Identifier: BSD-3-Clause
% Copyright(c) 2017 Intel Corporation. All rights reserved.
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

d = [];
nt = [];
nt_use = [];
nt_skip = [];

%% Use channel with strongest signal
ch = select_channel(x0);
x = x0(:, ch);

%% Find start marker
fprintf('Finding test start marker...\n');
s = sync_chirp(test.fs, 'up');
nx = length(x);
n_seek = round(test.fs*(test.idle_t + test.mark_t));
n = min(max(round(test.fs*test.sm), n_seek), nx);
y = x(1:n);
[r, lags] = xcorr(y, s);
r2 = r.^2;
r2_thr = 0.1 * max(r2);
idx = find(r2 > r2_thr);
d_start = lags(idx(1));

%% Find end marker
fprintf('Finding test end marker...\n');
s = sync_chirp(test.fs, 'down');
n_seek = round(test.fs*(test.idle_t + test.mark_t));
n = min(max(round(test.fs*test.em),n_seek), nx);
y = x(end-n+1:end);
[r, lags] = xcorr(y, s);
r2 = r.^2;
r2_thr = 0.1 * max(r2);
idx = find(r2 > r2_thr);
d_end = nx-n+lags(idx(end));

%% Check correct length of signal
len = d_end-d_start;
len_s = len/test.fs;
ref_s = test.mark_t+test.nf*test.na*test.tl;
if abs(len_s-ref_s) > test.mt
        fprintf(1, 'Start and end markers were not found. Signal quality may be poor.\n');
	return
end

%% Delay to first tone, length of tone in samples
d = d_start + round(test.mark_t*test.fs);
if (d < 0)
	fprintf(1, 'Invalid delay value. Signal quality may be poor.\n');
	return
	d = [];
end
nt = round(test.tl*test.fs);
nt_use = nt -round(test.is*test.fs) -round(test.ie*test.fs);
if nt_use < 0
        fprintf(1, 'Test signal ignore start/end mismatch.\n');
	d = [];
	nt = [];
	nt_use = [];
	return
end
nt_skip = round(test.is*test.fs);

end

%% Select channel function
function ch = select_channel(x)
	rms = get_rms(x);
	[~, ch] = max(rms);
end

%% RMS level in decibels
function rms_db = get_rms(x)
	s = size(x);
	nch = s(2);
	rms_db = zeros(1, nch);
	for i = 1:nch
		rms_db(i) = 20*log10(sqrt(mean(x(:,i).^2)));
	end
end
