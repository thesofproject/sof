function test = chirp_test_analyze(test)

% SPDX-License-Identifier: BSD-3-Clause
% Copyright(c) 2017-2020 Intel Corporation. All rights reserved.
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

test.ph = [];
test.fh = [];

%% Load output file
[x, nx] = load_test_output(test);
if nx < 1
	test.fail = -1; % No data
	return;
end

%% Find sync
[d, nt, nt_use, nt_skip] = find_test_signal(x(:,test.ch(1)), test);

%% Trim sample
i1 = d+nt_skip;
i2 = i1+nt_use-1;
z = x(i1:i2, :);
y = z(:,test.ch);
s = sum(z,2);

%% Quick check length, RMS, offset, and channels sum
sz = size(z);
tz = sz(1)/test.fs;
et = abs(test.cl-tz)/test.cl;
rms_db = 10*log10(mean(z.^2)) + 20*log10(sqrt(2));
offs_db = 20*log10(abs(mean(z)));
sum_rms_db = 10*log10(mean(s.^2));
% Check for proper ratio of out/in samples, minimum level, maximum offset
% and maximum of sum of channels. The input is such that the channels should
% sum to zero. A phase difference in channels would cause non-zero output
% for channels sum. Dithered input causes a small non-zero sum value.

if et > 0.05
	fail = 1;
	fprintf('Failed output chirp length, err=%f, t=%f.\n', et, tz);
else if (min(rms_db) < -6) && (test.fs2 + 1 > test.fs1)
	     fail = 1;
	     fprintf('Failed output chirp level.\n');
     else if max(offs_db) > -40
		  fail = 1;
		  fprintf('Failed output chirp DC offset.\n');
	  else if (sum_rms_db > -80) && (mod(test.nch, 2) == 0)
		       fail = 1;
		       fprintf('Failed output chirp channels phase.\n');
	       else
		       fail = 0;
	       end
	  end
     end
end

test.fh = figure('visible', test.plot_visible);
test.ph = subplot(1, 1, 1);
ns = 1024;
no = round(0.9*ns);
specgram(y(:,1), ns, test.fs, kaiser(ns,27), no);
colormap('jet');
caxis([-150 0]);
colorbar('EastOutside');

test.fail = fail;

end
