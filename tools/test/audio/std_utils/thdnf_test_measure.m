function test = thdnf_test_measure(test)

% SPDX-License-Identifier: BSD-3-Clause
% Copyright(c) 2017 Intel Corporation. All rights reserved.
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

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
	if max(max(test.thdnf)) > test.thdnf_max
		test.fail = 1;
	end
end

%% Do plots
for i = 1:length(test.ch)
	test.fh(i) = figure('visible', test.plot_visible);
	test.ph(i) = subplot(1, 1, 1);

	if test.plot_channels_combine
		semilogx(test.f, test.thdnf_high, test.f, test.thdnf_low, '--');
	else
		semilogx(test.f, test.thdnf_high(:,i), ...
			test.f, test.thdnf_low(:,i), '--');
	end

	grid on;
	if ~isempty(test.plot_thdn_axis);
		axis(test.plot_thdn_axis);

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
