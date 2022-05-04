% Compare SOF MFCC concept to pytorch's Kaldi version

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2022, Intel Corporation. All rights reserved.

input_t = 1.0;
input_fs = 16e3;
frames = floor(input_t * input_fs);

fn = 'test_chirp.wav'; % Create reference input file
if exist(fn, 'file') ~= 2
	t = (0:frames - 1)/input_fs;
	f0 = 20;
	f1 = input_fs / 2;
	x = chirp(t, f0, t(end), f1, 'linear')';
	audiowrite(fn, x, input_fs);
end

for a = -90:10:0
	fn = sprintf('test_sine_%ddb.wav', a); % Create reference input file
	if exist(fn, 'file') ~= 2
		t = (0:frames - 1)/input_fs;
		f0 = 625; % Rectangular window test for max FFT output
		x = 10^(a/20) * sin(2*pi*t*f0)';
		if a < 0
			x = floor(2^15 * x + rand(frames,1) + rand(frames,1))/2^15;
		end
		x(x > 1) = 1;
		x(x < -1) = -1;
		audiowrite(fn, x, input_fs);
	end
end

%% Run concept
fn = 'test_chirp.wav';
%fn = 'test_sine_-40db.wav';
%fn = 'speech.wav';

params = mfcc_defaults();
params.preemphasis_coefficient = 0;
params.remove_dc_offset = false;
params.window_type = 'hamming';
[test_coef, t, n] = test_mfcc(fn, params);

%% Run reference
[ref_coef, t, n] = test_mfcc_kaldi(fn);

%% Get delta, plot
delta=ref_coef - test_coef;
err_rms = sqrt(mean(mean(delta.^2)))

figure
surf(t, n, delta, 'EdgeColor', 'none');
view(45,60);
xlabel('Time (s)');
ylabel('Delta Mel band cepstral coefficients')
tstr = sprintf('SOF MFCC vs. Pytorch Kaldi RMS err %.3f', err_rms);
title(tstr);
