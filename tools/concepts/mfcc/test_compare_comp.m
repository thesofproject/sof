% Compare SOF MFCC concept to pytorch's Kaldi version

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2022, Intel Corporation. All rights reserved.

run_mfcc_path = '../../tune/mfcc';

% Run concept
fn = 'test_chirp.wav';
%fn = 'test_sine_-40db.wav';
%fn = 'speech.wav';

%params = mfcc_defaults();
%params.preemphasis_coefficient = 0;
%params.remove_dc_offset = false;
%params.window_type = 'hamming';
%[test_coef, b, n] = test_mfcc(fn, params);

cmd = sprintf('%s/run_mfcc.sh %s', run_mfcc_path, fn);
[status, output] = system(cmd);
if status ~= 0
	error('Failed cmd.');
end
path(path(), '../../tune/mfcc');
[test_ceps, t, n] = decode_ceps('mfcc.raw', 13);

% Run reference
[ref_coef, t, n] = test_mfcc_kaldi(fn);

delta=ref_coef - test_ceps;
err_rms = sqrt(mean(mean(delta.^2)))

% Get delta, plot
figure
surf(t, n, delta, 'EdgeColor', 'none');
view(45,60);
ylabel('Time (s)');
xlabel('Delta Mel band cepstral coefficients')
tstr = sprintf('SOF MFCC vs. Pytorch Kaldi RMS err %.3f', err_rms);
title(tstr);
