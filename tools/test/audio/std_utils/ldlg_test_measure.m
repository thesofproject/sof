function test = ldlg_test_measure(test)

% SPDX-License-Identifier: BSD-3-Clause
% Copyright(c) 2017 Intel Corporation. All rights reserved.
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

%% Reference: AES17 8.1 Level dependent logarithmic gain
%  http://www.aes.org/publications/standards/

default_result = NaN * ones(test.na, length(test.ch));
test.gain_db = default_result;

%% Load output file
[x, nx] = load_test_output(test);

if nx == 0
	test.fail = 1;
	return
end

%% 1/3 octave filter
fc = test.f;
bw = 1/3;
fmin = fc / sqrt(2^bw);
fmax = fmin * 2^bw;
wn = 2 * [fmin fmax] / test.fs;
[b, a] = butter(4, wn);
y0 = filter(b, a, x);

%% Find sync
[d, nt, nt_use, nt_skip] = find_test_signal(x, test);
if isempty(d)
	test.fail = 1;
	return
end

%% Measure gains for sweep
level_in = test.a_db(:);
for n=1:test.na
	i1 = d + (n-1) * nt + nt_skip;
	i2 = i1 + nt_use - 1;
	level_out(n,:) = level_dbfs(y0(i1:i2,:));
end

test.gain_db = level_out - level_in + test.att_rec_db;
test.level_in_db = level_in;
test.level_out_db = level_out + test.att_rec_db;
test.fail = 0;

test.fh(1) = figure('visible', test.plot_visible);
test.ph(1) = subplot(1, 1, 1);
if test.plot_channels_combine
	plot(level_in, level_out);
	hold on
	p1 = min(level_in);
	p2 = max(level_in);
	plot([p1 p2], [p1 p2], 'k--');
	hold off
	grid on
	xlabel('Input level (dBFS)');
	ylabel('Output level (dBFS)');
	if test.nch == 2
		legend('ch1', 'ch2', 'Location', 'NorthWest');
	end
end

test.fh(2) = figure('visible', test.plot_visible);
test.ph(2) = subplot(1, 1, 1);
if test.plot_channels_combine
	plot(level_in, test.gain_db);
	hold on
	plot([p1 p2], [0 0], 'k--');
	hold off
	grid on
	xlabel('Input level (dBFS)');
	ylabel('Gain (dB)');
	if test.nch == 2
		legend('ch1', 'ch2');
	end
end
