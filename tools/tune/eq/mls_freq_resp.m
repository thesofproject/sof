function [f, m_db] = mls_freq_resp(id)
%% Measure frequency response with MLS test signal
%
%  [f, m] = mls_freq_resp(id)
%
%  Input parameters
%  id - A string identifier for test case. An id 'selftest' is for special
%       usage. It calculates response of filtered MLS signal and computes
%       the measurement vs. known. Deviation is reported as error. It can
%       be useful if the internal MLS measurement parameters are adjusted.
%
%  Output parameters
%  f - Frequency vector in Hz
%  m - Measured magnitude responses in dB
%
%  Configuration (edit these):
%  mls_play_config.txt
%  mls_rec_config.txt
%
%  The script will return also a text CSV format file with name mls-<id>.txt.
%

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2018-2020, Intel Corporation. All rights reserved.
%
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

%% Settings
np = 1024;                  % Number of frequency points to use
f_lo = 100;                 % Lower frequency limit for analysis
f_hi = 20e3;                % Upper frequency limit for analysis
t_tot = 10e-3;              % MLS analysis window length in s
t_mls_s = 1.0;              % MLS test signal length in s
a_mls_db = -10;             % MLS test signal amplitude in dB
fs = 48e3;                  % Sample rate in Hz
bits = 16;                  % Audio format to use (bits)
fmt = 'S16_LE';             % Audio format to use (ALSA)
dir = '/tmp';               % Directory for temporary files
capture_level_max_db = -1;  % Expected max. level
capture_level_min_db = -30; % Expacted min. level

%% Get device identifier to use
if nargin < 1
	id = 'unknown';
end

if strcmp(id, 'selftest')
	selftest = 1;
	% Just some simulated speaker response to use as self test case
	stb = [  0.341762453, -0.915611126,  0.482465118, ...
	         1.017612317, -1.722527013,  0.711608745, ...
	         0.630608859, -0.813609935,  0.267690582, ];
	sta = [  1.000000000, -3.931695128,  6.630812276, ...
	        -6.339248735,  3.800407709, -1.559376698, ...
	         0.619626250, -0.317702349,  0.097453451, ];
else
	selftest = 0;
end
measfn = sprintf('mls-%s.wav', id);
csvfn = sprintf('mls-%s.txt', id);

%% Paths
addpath('../../test/audio/test_utils');

%% MLS
n_mls = round(fs*t_mls_s);
mls = 10^(a_mls_db/20) * (2 * mlsp12(1, n_mls) - 1);
mlsfn = 'mls-ref.wav';
audiowrite(mlsfn, mls, fs);

%% Chip markers and parameters for find sync
[x1, m1] = sync_chirp(fs, 'up');
[x2, m2] = sync_chirp(fs, 'down');
fnd.fs = fs; % Sample rate
fnd.sm = 5; % Max seek from start
fnd.em = 3; % Max seek from end
fnd.idle_t = 2; % max idle in start or end
fnd.mark_t = m1.t; % Marker length
fnd.nf = 1; % One signal (amplitude)
fnd.na = 1; % One signal (frequency)
fnd.tl = t_mls_s; % Length of signal
fnd.mt = 0.1; % Threshold length to issue error
fnd.is = 0; % Ignore from start
fnd.ie = 0; % Ignore from end

%% Merge markers and MLS
z = zeros(n_mls + m1.n + m2.n, 1);
i1 = m1.n + 1;
i2 = m1.n + n_mls;
z(1:i1 - 1) = x1;
z(i1:i2) = mls;
z(i2 + 1:end) = x2;

%% Get config
rec_cfg = meas_remote_rec_config(fs, fmt);
play_cfg = meas_remote_play_config;

%% Capture MLS from all playback channel at time
mixfn = 'mlsmix.wav';
recfn = 'recch.wav';
y = [];
for i=1:play_cfg.nch
	fprintf('\n');
	fprintf('Measure playback channel %d\n', i);
	if selftest
		tz =zeros(2*fs+length(z), 1); % Pad 2s
		tz(fs:fs+length(z)-1) = z;
	        t = filter(stb, sta, tz); % Filter with test response
		r = t(:) * ones(1, rec_cfg.nch); % Copy to all channels
	else
		x = zeros(length(z), play_cfg.nch);
		x(:,i) = z;
		mixdfn = sprintf('%s/%s', dir, mixfn);
		audiowrite(mixdfn, x, fs, 'BitsPerSample', bits);
		copy_playback(mixdfn, play_cfg);
		tcap = floor(6 + t_mls_s); % Capture for MLS +6s
		remote_capture(recfn, rec_cfg, tcap);
		pause(1);
		remote_play(mixfn, play_cfg);
		pause(3);
		r = get_recording(recfn, rec_cfg);
	end
	[d, nt] = find_test_signal(r(:,1), fnd);
	if isempty(d)
		figure;
		sr = size(r);
		ts = (0:sr(1)-1)/fs;
		plot(ts, r(:,1));
		grid on;
		xlabel('Time (s)');
		ylabel('Sample value');
		title('Captured audio test waveform');
		fprintf(1, 'Error: check the plot for skew in capture/playback.\n');
		f = [];
		m_db = [];
		return
	end
	for j = 1:rec_cfg.nch
		y(:, rec_cfg.nch*(i-1) + j) = r(d:d + nt -1, j);
	end
	m = 20*log10(max(abs(r)));
	fprintf('Peak levels for capture channels (dB):');
	for j = 1:rec_cfg.nch
		fprintf(' %5.1f', m(j));
	end
	fprintf('\n');
	if max(m) > capture_level_max_db
		fprintf('Warning: The recording level is too loud.\n');
	end
	if min(m) < capture_level_min_db
		fprintf('Warning: The recording level is too silent.\n');
	end

end
audiowrite(measfn, y, fs, 'BitsPerSample', bits);
fprintf('\n');
fprintf('Done.\n');

[f, m_db,  b] = mls_calc_resp(csvfn, mlsfn, measfn, t_tot, np, f_lo, f_hi);

[f, m_db] = apply_mic_calibration(f, m_db, rec_cfg);

figure
idx = find(f>1e3, 1, 'first') - 1;
m_db_align = m_db - m_db(idx);
semilogx(f, m_db_align);
ax=axis(); axis([f_lo f_hi ax(3:4)]);
grid on;
xlabel('Frequency (Hz)');
ylabel('Magnitude (dB)');

if selftest
	title('Measured vs. reference response');
	h = freqz(stb, sta, f, fs);
        ref_db = 20*log10(abs(h));
        ref_db_align = ref_db - ref_db(idx);
	hold on;
	plot(f, ref_db_align, 'r--');
	hold off;
        idx = find(f < f_hi);
        idx = find(f(idx) > f_lo);
        m_lin = 10.^(m_db_align(idx)/20);
        ref_lin = 10.^(ref_db_align(idx)/20);
        err_lin = m_lin - ref_lin;
        e_rms = sqrt(mean(err_lin.^2));
        e_db = 20*log10(e_rms);
	figure;
	semilogx(f(idx), 20*log10(abs(err_lin)))
	grid on;
	xlabel('Frequency (Hz)');
	ylabel('Magnitude (dB)');
	title('Observed Error in self test');
        if e_db < -30
                fprintf('Passed self test. ');
        else
                fprintf('Failed self test. ');
        end
        fprintf('Response RMS error is %4.1f dB.\n', e_db);
end

end

function copy_playback(fn, cfg)
	if cfg.ssh
		cmd = sprintf('scp %s %s:%s/', fn, cfg.user, cfg.dir);
		fprintf('Remote copy: %s\n', cmd);
		system(cmd);
	else
		%cmd = sprintf('cp %s %s/', fn, cfg.dir);
		%fprintf('Local copy: %s\n', cmd);
	end
end

function y = get_recording(fn, cfg)
	if cfg.ssh
		cmd = sprintf('scp %s:%s/%s %s', cfg.user, cfg.dir, fn, fn);
		fprintf('Remote copy: %s\n', cmd);
	else
		cmd = sprintf('cp %s/%s %s', cfg.dir, fn, fn);
		fprintf('Local copy: %s\n', cmd);
	end
	system(cmd);
	y = audioread(fn);
	delete(fn);
end

function remote_play(fn, cfg)
	if cfg.ssh
		cmd = sprintf('ssh %s aplay -D%s %s/%s', cfg.user, cfg.dev, cfg.dir, fn);
		fprintf('Remote play: %s\n', cmd);
	else
		cmd = sprintf('aplay -D%s %s/%s', cfg.dev, cfg.dir, fn);
		fprintf('Local play: %s\n', cmd);
	end
	system(cmd);
end

function remote_capture(fn, cfg, t)
	if cfg.ssh
		cmd = sprintf('ssh %s arecord -q -D%s %s -d %d %s/%s &', ...
			      cfg.user, cfg.dev, cfg.fmt, t, cfg.dir, fn);
		fprintf('Remote capture: %s\n', cmd);
	else
		cmd = sprintf('arecord -q -D%s %s -d %d %s/%s &', ...
			      cfg.dev, cfg.fmt, t, cfg.dir, fn);
		fprintf('Local capture: %s\n', cmd);
	end
	system(cmd);
end

function play = meas_remote_play_config()
	source mls_play_config.txt;
	fprintf('\nThe setttings for remote playback are\n');
	fprintf('Use ssh   : %d\n', play.ssh);
	fprintf('User      : %s\n', play.user);
	fprintf('Directory : %s\n', play.dir);
	fprintf('Device    : %s\n', play.dev);
	fprintf('Channels  : %d\n', play.nch);
end

function rec = meas_remote_rec_config(fs, fmt)
	source mls_rec_config.txt;
	rec.fmt = sprintf('-t wav -c %d -f %s -r %d', ...
			     rec.nch, fmt, fs);

	fprintf('\nThe setttings for remote capture are\n');
	fprintf('Use ssh          : %d\n', rec.ssh);
	fprintf('User             : %s\n', rec.user);
	fprintf('Directory        : %s\n', rec.dir);
	fprintf('Device           : %s\n', rec.dev);
	fprintf('format           : %s\n', rec.fmt);
	fprintf('Channels         : %d\n', rec.nch);
	fprintf('Calibration Data : %s\n', rec.cal);

	if ~isempty(rec.cal)
		if exist(rec.cal, 'file')
			[rec.cf, rec.cm, rec.sens, rec.cs] = get_calibration(rec.cal);
		else
			error('The calibration file does not exist');
		end
	else
		rec.cf = [];
		rec.cm = [];
		rec.sens = [];
		rec.cs = '';
	end
end

% The syntax of ASCII text calibration data is such that first line is
% text string sometimes within "" or text line about Sensitivity. Such
% lines are read and stored to description text. The sensitivty is extracted
% and printed if success but currently it is not utilized by the code.
%
% The next lines are <frequency Hz> <magnitude decibels> for
% measurement data for microphone. If there are more than two numbers
% per line the other than two first columns are ignored. This code
% applies inverse of calibration data for measured response calibration.

function [f, m, sens, desc] = get_calibration(fn)
	fh = fopen(fn, 'r');
	if fh < 0
		error('Cannot open calibration data file');
	end
	n = 1;
	f = [];
	m = [];
	sens =[];
	desc = '';
	str = fgets(fh);
	idx = findstr(str, '"');
	while length(idx) > 0
		line = str(idx(1)+1:idx(2)-1);
		desc = sprintf('%s%s ', desc, line);
		str = fgets(fh);
		idx = findstr(str, '"');
	end
	if length(strfind(str, 'Sens'))
		desc = str;
		str = fgets(fh);
	end
	while str ~= -1
		d=sscanf(str,'%f');
		f(n) = d(1);
		m(n) = d(2);
		n = n + 1;
		str = fgets(fh);
	end

	% Strip possible linefeed from description end
	if double(desc(end)) == 10
		desc = desc(1:end-1);
	end
	fprintf('Calibration Info : %s\n', desc);
	i1 = strfind(desc, 'Sens Factor =');
	i2 = strfind(desc, 'dB');
	if length(i1) == 1 && length(i2) == 1
		sens = sscanf(desc(i1+13:i2-1), '%f');
		fprintf('Calibration Sens : %6.2f dB\n', sens);
	end

	fprintf('Calibration range: %.2f .. %.2f Hz\n', min(f), max(f));
	fprintf('Calibration range: %.2f .. %.2f dB\n', min(m), max(m));

	figure;
	semilogx(f, m);
	grid on;
	title(desc);
	xlabel('Frequency (Hz)');
	ylabel('Magnitude (dB)');
end

function [x, seed] = mlsp12(seed, n)

% Based on book Numerical Recipes in C, chapter 7.4 Generation of
% random bits method II p. 298-299 example and 12 bit primitive
% polynomial (12, 6, 4, 1, 0)

	x = zeros(1,n);
	ib1 = 2^(1-1);
	ib4 = 2^(4-1);
	ib6 = 2^(6-1);
	ib12 = 2^(12-1);
	mask = ib1 + ib4 + ib6 + ib12;

	for i = 1:n
		if bitand(seed, ib12)
			seed = bitor(bitxor(seed, mask) * 2 , ib1);
			x(i) = 1;
		else
			seed = seed * 2;
			x(i) = 0;
		end
	end
end

%% Calibration apply function
%  Resample microphone calibration data into used grid and
%  then subtract calibration response from measured
%  response.

function [cal_f, cal_m_db] = apply_mic_calibration(f, m_db, rec)

	if length(rec.cm) > 0
		if ~isvector(rec.cm)
			error('Calibration can be for one channel only');
		end
		mic_m_db = interp1(rec.cf, rec.cm, f, 'linear');
		nans = isnan(mic_m_db);
		idx = find(nans == 0);
		cal_f = f(idx);
		cal_db = mic_m_db(idx);
		s = size(m_db);
		for i = 1:s(2)
			cal_m_db(:,i) = m_db(idx,i) - cal_db(:);
		end
	else
		cal_m_db = m_db;
		cal_f = f;
	end

end
