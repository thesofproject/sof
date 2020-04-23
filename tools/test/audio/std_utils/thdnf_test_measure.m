function test = thdnf_test_measure(test)

% SPDX-License-Identifier: BSD-3-Clause
% Copyright(c) 2017 Intel Corporation. All rights reserved.
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

test.ph = [];
test.fh = [];

if isempty(test.thdnf_mask_f)
	if ~isempty(test.thdnf_max)
		test.thdnf_mask_f = [1 test.fs/2]; % Start from 1 due to log()
		test.thdnf_mask_hi = test.thdnf_max * [1 1];
	end
else
	if ~isempty(test.thdnf_max)
		error('Set either thdnf_max or thdnf_mask_f & thdnf_mask_hi but not both');
	end
	if isempty(test.thdnf_mask_hi)
		error('thdnf_mask_hi must be set when thdnf_mask_f is defined');
	end
end

%% Reference: AES17 6.3.2 THD+N ratio vs frequency
%  http://www.aes.org/publications/standards/

default_result = NaN * ones(test.nf, length(test.ch));
test.thdnf_high = default_result;
test.thdnf_low = default_result;

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

%% Interpolate THD+N mask
idx = find(test.f <= test.thdnf_mask_f(end));
idx = find(test.f(idx) >= test.thdnf_mask_f(1));
f_mask = test.f(idx);
f_log = log(f_mask);
mask_hi = interp1(log(test.thdnf_mask_f), test.thdnf_mask_hi, f_log, 'linear');
mask_hi = mask_hi(:);

%% Measure all test frequencies
test.fail = 0;
for i = 1:length(test.ch);
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
			y0n = stdnotch(y0(i1:i2, i), test.f(n), test.fs);
			ml(n,m) = level_dbfs(y0(i1:i2, i));
			mn(n,m) = level_dbfs(y0n(n_notch_settle:end));
		end
	end

	test.thdnf = mn - ml;
	test.thdnf_high(:,i) = test.thdnf(:,1);
	test.thdnf_low(:,i) = test.thdnf(:,2);

	fidx = find(test.thdnf(idx, 1) > mask_hi);
	if length(fidx) > 0
		fail_hi = 1;
		fprintf('Failed THD+N mask with high input.\n');
	else
		fail_hi = 0;
	end

	fidx = find(test.thdnf(idx, 2) > mask_hi);
	if length(fidx) > 0
		fail_lo = 1;
		fprintf('Failed THD+N mask with low input.\n');
	else
		fail_lo = 0;
	end

	if fail_hi && fail_lo
		test.fail = 1;
	end
end

%% Do plots
for i = 1:length(test.ch)
	test.fh(i) = figure('visible', test.plot_visible);
	test.ph(i) = subplot(1, 1, 1);

	if test.plot_channels_combine
		semilogx(test.f, test.thdnf_high, test.f, test.thdnf_low);
	else
		semilogx(test.f, test.thdnf_high(:,i), ...
			test.f, test.thdnf_low(:,i));
	end

	if ~isempty(test.thdnf_mask_f)
		hold on
		plot(f_mask, mask_hi, 'k--');
		hold off
	end

	grid on;
	if ~isempty(test.plot_thdn_axis)
		axis(test.plot_thdn_axis);
	end

	xlabel('Frequency (Hz)');
	ylabel('THD+N (dB)');

	if test.plot_channels_combine && (test.nch > 1)
		switch test.nch
			case 2
				ls1 = sprintf("ch%d  -1 dBFS", test.ch(1));
				ls2 = sprintf("ch%d  -1 dBFS", test.ch(2));
				ls3 = sprintf("ch%d -20 dBFS", test.ch(1));
				ls4 = sprintf("ch%d -20 dBFS", test.ch(2));
				legend(ls1, ls2, ls3, ls4, 'Location', 'NorthEast');
		end
	else
		ls1 = sprintf("ch%d  -1 dBFS", test.ch(i));
		ls2 = sprintf("ch%d -20 dBFS", test.ch(i));
		legend(ls1, ls2, 'Location', 'NorthEast');
	end

	if test.plot_channels_combine
		break
	end

end

end
