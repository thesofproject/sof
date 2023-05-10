function test = fullscale_test_measure(test)

% SPDX-License-Identifier: BSD-3-Clause
% Copyright(c) 2023 Intel Corporation. All rights reserved.
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

default_result = NaN * ones(1,length(test.ch));
test.fail = 1;

%% Load output file
%test.fn_out = test.fn_in;
[x0, nx] = load_test_output(test);
if nx == 0
	return
end

%% Find sync
[d, nt, nt_use, nt_skip] = find_test_signal(x0, test);
if isempty(d)
	return
end

%% Trim sample by removing first 1s to let the notch to apply
i1 = d+nt_skip;
i2 = i1+nt_use-1;
x = x0(i1:i2, :);

% STFT and measure parameters
param.min_snr = 6 * test.bits_in;
param.max_snr_drop = 1;
param.t_ignore_from_start = 0;
param.t_ignore_from_end = 0;
param.level_differential_tol = 1e-3;
param.t_step = 1e-3;
param.n_fft = 2048;
param.visible = 'on';
param.do_plot = 1;
param.do_print = 0;

for ch = 1 : test.nch
	% Sanity check
	if ~signal_found(x(:, ch))
		fprintf(1, 'Error: Channel %d has no signal\n', ch);
		return
	end

	% STFT
	[stft, param] = compute_stft(x(:, ch), test.fs, param);

	% Check frequency, get STFT frequency index
	[signal_idx, tone_f] = find_test_tone(stft, param, test.f);

	% Get levels and SNR estimates
	meas = get_tone_levels(stft, param, signal_idx);

	% Checks for levels data from FFTs, after stable level
	% and time to ignore
	meas = check_tone_levels(param, meas);

	all_stft(ch, :, :) = stft;
	all_meas(ch) = meas;
	if meas.success == false
		test_passed = false;
	end

end

plot_specgram(param, all_stft, test.nch, 'Full scale sine');
plot_levels(param, all_meas, test.nch, 'Full scale sine');

end

function [stft, param] = compute_stft(x, fs, param)

sx = size(x);
if sx(2) > 1
	error('One channel only');
end

frames = sx(1);
win = kaiser(param.n_fft, 20);
param.win_spread = 7;
param.win_gain = -13.0379;
param.fs = fs;

param.n_step = fs * param.t_step;
param.n_stft = floor((frames - param.n_fft) / param.n_step);
n_half_fft = param.n_fft / 2 + 1;
scale = 1 / param.n_fft;
param.f = linspace(0, fs/2, n_half_fft);
param.t = (0 : (param.n_stft - 1)) * param.t_step;
stft = zeros(n_half_fft, param.n_stft);

for i = 1 : param.n_stft
	i1 = (i - 1) * param.n_step + 1;
	i2 = i1 + param.n_fft - 1;
	s1 = fft(x(i1 : i2) .* win) * scale;
	s2 = s1(1 : n_half_fft);
	s = s2 .* conj(s2);
	stft(:, i) = s;
end

end

function meas = get_tone_levels(stft, param, signal_idx)

signal_i1 = signal_idx - param.win_spread;
signal_i2 = signal_idx + param.win_spread;
if signal_i1 < 1
	error('Too low tone frequency, increase FFT length');
end

signal_db = zeros(param.n_stft, 1);
noise_db = zeros(param.n_stft, 1);
snr_db = zeros(param.n_stft, 1);
for i = 1 : param.n_stft
	% Integrate signal power
	p_signal = sum(stft(signal_i1 : signal_i2, i));

	% Integrate noise power, but replace DC and signal with
	% average noise level.
	noise = stft(:, i);
	noise_avg = mean(noise(signal_i2 : end));
	noise(1 : param.win_spread) = noise_avg;
	noise(signal_i1 : signal_i2) = noise_avg;
	p_noise = sum(noise);

	% Sign, noise, and "SNR" as dB
	signal_db(i) = 10*log10(p_signal);
	noise_db(i) = 10*log10(p_noise);
	snr_db(i) = signal_db(i) - noise_db(i);
end

meas.noise_db = noise_db - param.win_gain;
meas.signal_db = signal_db - param.win_gain;
meas.snr_db = signal_db - noise_db;

end

function meas = check_tone_levels(param, meas)

meas.t_glitch = 0;
meas.num_glitches = 0;
meas.success = true;

% Find when level stabilizes, from start ramp. Signal level is
% stable where differential of level is less than required
% tolerance.
da = abs(diff(meas.signal_db));
i0 = find(da < param.level_differential_tol, 1, 'first');
if isempty(i0)
	error('Signal level is not stable');
end

i1 = i0 + param.t_ignore_from_start / param.t_step;
i2 = param.n_stft - param.t_ignore_from_end / param.t_step;

meas.t_start = (i1 - 1) * param.t_step;
meas.t_end = (i2 - 1) * param.t_step;
meas.mean_signal_db = mean(meas.signal_db(i1 :i2));
meas.mean_noise_db = mean(meas.noise_db(i1 :i2));
meas.mean_snr_db = mean(meas.snr_db(i1 :i2));
meas.max_noise_db = max(meas.noise_db(i1 :i2));
meas.min_snr_db = min(meas.snr_db(i1 :i2));

% Find glitches from SNR curve drops
idx = find(meas.snr_db(i1:i2) < meas.mean_snr_db - param.max_snr_drop);

if ~isempty(idx)
	idx = idx + i1 - 1;
	didx = diff(idx);
	meas.num_glitches = 1 + length(find(didx > 2));
	start_idx = idx(1);
	cidx = find(didx(1:end) > 1, 1, 'first');
	if isempty(cidx)
		end_idx = idx(end);
	else
		end_idx = idx(cidx);
	end
	meas.t_glitch = param.t_step * mean([start_idx end_idx] - 1) + ...
		0.5 * param.n_fft / param.fs;
	meas.success = false;
end

if meas.min_snr_db < param.min_snr
	meas.success = false;
end

end

function [signal_idx, tone_f] = find_test_tone(stft, param, test_tone_f)

if test_tone_f > 0
	err_ms = (param.f - test_tone_f) .^ 2;
	signal_idx = find(err_ms == min(err_ms));
	tone_f = param.f(signal_idx);
	return
end

i1 = 1 + param.t_ignore_from_start / param.t_step;
i2 = param.n_stft - param.t_ignore_from_end / param.t_step;
signal_idx_all = zeros(i2 - i1 + 1, 1);
for i = i1 : i2
	signal_idx_all(i - i1 + 1) = find(stft(:, i) == max(stft(:, i)),1, 'first');
end

signal_idx = round(mean(signal_idx_all));
tone_f = param.f(signal_idx);

end

function success = signal_found(x)

% All zeros or DC
if abs(min(x) - max(x)) < eps
	success = 0;
else
	success = 1;
end

end

function fh = plot_specgram(param, all_stft, channels, fnstr)

if param.do_plot
	fh = figure('Visible', param.visible);
	for n = 1 : channels
		subplot(channels, 1, n);
		clims = [-140 0];
		stft = squeeze(all_stft(n, :, :));
		imagesc(1e3 * param.t, param.f, 10*log10(stft + eps), clims);
		set(gca, 'ydir', 'normal');
		colorbar;
		grid on;
		if n == 1
			title(fnstr, 'interpreter', 'none');
		end
		lstr = sprintf('Ch%d freq (Hz)', n);
		ylabel(lstr);
	end
	xlabel('Time (ms)');
	if param.do_print
		pfn = sprintf('%s/%s_specgram.png', param.print_dir, fnstr);
		print(pfn, '-dpng');
	end

end

end

function fh = plot_levels(param, meas, channels, fnstr)

if param.do_plot
	t_ms = 1e3 * param.t;
	fh = figure('Visible', param.visible);
	subplot(3, 1, 1);
	hold on;
	vmin = 1000;
	vmax = -1000;
	for n = 1 : channels
		plot(t_ms, meas(n).snr_db);
		vmin = min(min(meas(n).snr_db), vmin);
		vmax = max(max(meas(n).snr_db, vmin));
	end
	plot(1e3 * [meas(1).t_start meas(1).t_end], [param.min_snr param.min_snr], '--');
	hold off
	y_min = floor(vmin / 10) * 10;
	y_max = ceil(vmax / 10) * 10 + 40;
	axis([0 t_ms(end) y_min y_max]);
	grid on;
	ylabel('SNR (dB)');
	title(fnstr, 'interpreter', 'none');
	ch1 = sprintf('ch1 %.0f dB avg %.0f dB min', ...
		meas(1).mean_snr_db, meas(1).min_snr_db);
	switch channels
		case 1
			legend(ch1);
		otherwise
			ch2 = sprintf('ch1 %.0f dB avg %.0f dB min', ...
				meas(2).mean_snr_db, meas(2).min_snr_db);
			legend(ch1, ch2);
	end

	subplot(3, 1, 2);
	hold on
	vmin = 1000;
	vmax = -1000;
	for n = 1 : channels
		plot(t_ms, meas(n).signal_db);
		vmax = max(max(meas(n).signal_db), vmax);
		vmin = min(min(meas(n).signal_db), vmin);
	end
	hold off
	y_min = floor(vmin) - 1;
	y_max = ceil(vmax) + 1;
	axis([0 t_ms(end) y_min y_max]);
	grid on;
	ylabel('Signal (dBFS)');
	subplot(3, 1, 3);
	hold on
	vmin = 1000;
	vmax = -1000;
	for n = 1 : channels
		plot(t_ms, meas(n).noise_db);
		vmax = max(max(meas(n).noise_db), vmax);
		vmin = min(min(meas(n).noise_db), vmin);
	end
	hold off
	y_min = floor(vmin / 10) * 10;
	y_max = ceil(vmax / 10) * 10;
	axis([0 t_ms(end) y_min y_max]);
	grid on;
	ylabel('Noise (dBFS)');
	xlabel('Time (ms)');

	if param.do_print
		pfn = sprintf('%s/%s_level.png', param.print_dir, fnstr);
		print(pfn, '-dpng');
	end

end

end
