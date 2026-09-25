% Compare SOF MFCC concept to librosa

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2022, Intel Corporation. All rights reserved.

% Run concept
fn = 'test_chirp.wav';
%fn = 'test_sine.wav';
%fn = 'speech.wav';

params = mfcc_defaults();
params.preemphasis_coefficient = 0;
params.remove_dc_offset = false;
params.window_type = 'hann';
params.norm = 'slaney';
params.cepstral_lifter = 0;
params.num_mel_bins = 13;
params.low_freq = 0;
params.mel_log = 'MEL_DB';
params.pmin = 1e-10;
params.top_db = 80;

%% Run this MFCC
[test_coef, t, n] = test_mfcc(fn, params);

%% Run reference
[ref_coef, rt, rn] = test_mfcc_librosa(fn);

%% Get delta, plot
ref_coef = ref_coef(:, 2:end-2);
delta = ref_coef - test_coef;
err_rms = sqrt(mean(mean(delta.^2)))

% Get delta, plot
figure
surf(t, n, delta, 'EdgeColor', 'none');
view(45,60);
xlabel('Time (s)');
ylabel('Delta Mel band cepstral coefficients')
tstr = sprintf('SOF MFCC vs. librosa RMS err %.3f', err_rms);
title(tstr);
