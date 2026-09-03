% state = mfcc_get_mel_filterbank(param, stream, state)
%
% Input
%   param.sample_frequency
%   param.high_freq
%   param.low_frew
%   param.num_mel_bins
%   param.nowm - use 'slaney' or 'none'
%   param.mel_log - use 'log', 'log10', 'db'
%   state.fft_padded_size - e.g. 512
%   state.half_fft_size - e.g. 257
%
% Output
%   state.triangles - Mel filter bank coefficients

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2022, Intel Corporation. All rights reserved.

function state = mfcc_get_mel_filterbank(param, state)

plot_triangles = 0;
plot_matlab_triangles = 0;
fn = param.sample_frequency / 2;
state.fft_f = linspace(0, fn, state.half_fft_size);

% Mel frequencies
if param.high_freq == 0
	high_freq = fn;
else
	high_freq = param.high_freq;
end

if param.low_freq < 0 || param.low_freq > fn || high_freq > fn || param.low_freq > high_freq
	error('Illegal start/end frequencies');
end

ms = mfcc_hz_to_mel(param.low_freq);
me = mfcc_hz_to_mel(high_freq);

fft_bin_width = param.sample_frequency / state.fft_padded_size;
mel_freq_delta = (me - ms) / (param.num_mel_bins + 1);
mel = mfcc_hz_to_mel((0:(state.fft_padded_size / 2)) * fft_bin_width);
for i = 0:param.num_mel_bins - 1
	left_mel = ms + i * mel_freq_delta;
	center_mel = ms + (i + 1) * mel_freq_delta;
	right_mel = ms + (i + 2) * mel_freq_delta;
	up_slope = (mel - left_mel) / (center_mel - left_mel);
	down_slope = (right_mel - mel) / (right_mel - center_mel);
	bins = min(up_slope, down_slope);
	bins(bins < 0) = 0;
	if param.norm_slaney
		left_hz = mfcc_mel_to_hz(left_mel);
		right_hz = mfcc_mel_to_hz(right_mel);
		s = 2 / (right_hz - left_hz);
	else
		s = 1;
	end
	state.triangles(:,i + 1) = s * bins;
end

% Pytorch uses log, librosa uses dB, matlab uses log10
switch param.mel_log
	case 'MEL_LOG'
		state.log_scale = 1;
	case 'MEL_LOG10'
		state.log_scale = 1/log(10);
	case 'MEL_DB'
		state.log_scale = 10/log(10);
	otherwise
		error('Invalid mel_log');
end

if plot_triangles
	figure;
	plot(state.triangles);
	grid on;
	xlabel('FFT bin');
	ylabel('Weight')
	title('This version computed');
end

if plot_matlab_triangles && exist('OCTAVE_VERSION', 'builtin') == 0
	% Reference in Matlab
	[fb,cf] = designAuditoryFilterBank(param.sample_frequency, ...
		"FFTLength", state.fft_padded_size, "NumBands", param.num_mel_bins, ...
		"FrequencyRange", [0 fn], "Normalization", 'bandwidth');
	figure;
	plot(fb');
	grid on;
	xlabel('FFT bin');
	ylabel('Weight')
	title('Matlab computed');
end

end
