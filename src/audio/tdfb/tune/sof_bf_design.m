% bf = sof_bf_design(bf)
%
% This script calculates beamformer filters with superdirective design
% criteria.

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2020, Intel Corporation. All rights reserved.
%
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

function bf = sof_bf_design(bf)

sof_bf_paths(true);
mkdir_check('plots');
mkdir_check('data');

if length(bf.steer_az) ~= length(bf.steer_el)
	error('The steer_az and steer_el lengths need to be equal.');
end

if max(bf.steer_az) > 180 || min(bf.steer_az) < -180
	error('The steer_az angles need to be -180 to +180 degrees');
end

if max(bf.steer_el) > 90 || min(bf.steer_el) < -90
	error('The steer_el angles need to be -90 to +90 degrees');
end

switch lower(bf.array)
	case 'line'
		bf = sof_bf_array_line(bf);
	case 'circular'
		bf = sof_bf_array_circ(bf);
	case 'rectangle'
		bf = sof_bf_array_rect(bf);
	case 'lshape'
		bf = sof_bf_array_lshape(bf);
	case 'xyz'
		bf = sof_bf_array_xyz(bf);
	otherwise
		error('Invalid array type')
end

if isempty(bf.num_filters)
	if isempty(bf.input_channel_select)
		bf.num_filters = bf.mic_n;
	else
		bf.num_filters = length(bf.input_channel_select);
	end
end

bf = sof_bf_array_rot(bf);

% The design function handles only single (az, el) value, so need to
% loop every steer angle.
bf.num_angles = length(bf.steer_az);
all_az = bf.steer_az;
all_el = bf.steer_el;
all_array_id = bf.array_id;
all_noiserot_fn = bf.noiserot_fn;
all_sinerot_fn = bf.sinerot_fn;
all_diffuse_fn = bf.diffuse_fn;
all_random_fn = bf.random_fn;
all_mat_fn = bf.mat_fn;

for n = 1:bf.num_angles
	bf.steer_az = all_az(n);
	bf.steer_el = all_el(n);
	bf.array_id = all_array_id{n};
	bf.noiserot_fn = all_noiserot_fn{n};
	bf.sinerot_fn = all_sinerot_fn{n};
	bf.diffuse_fn = all_diffuse_fn{n};
	bf.random_fn = all_random_fn{n};
	bf.mat_fn = all_mat_fn{n};
	bf = bf_one_design(bf);
	w_all(:,:,n) = bf.w;
end

bf.steer_az = all_az;
bf.steer_el = all_el;
bf.array_id = all_array_id;
bf.noiserot_fn = all_noiserot_fn;
bf.sinerot_fn = all_sinerot_fn;
bf.diffuse_fn = all_diffuse_fn;
bf.random_fn = all_random_fn;
bf.mat_fn = all_mat_fn;
bf.w = w_all;

sof_bf_paths(false);
end

function bf = bf_one_design(bf)

%% Defaults
j = complex(0,-1);
fs = bf.fs;
N = 1024;
N_half = N/2+1;
f = (0:N/2)*fs/N';
phi_rad = (-180:180)*pi/180;
phi_rad = phi_rad(1:end-1);
n_phi = length(phi_rad);
steer_az = bf.steer_az*pi/180;
steer_el = bf.steer_el*pi/180;
mu = ones(1,N_half) * 10^(bf.mu_db/20);
%% Source at distance r
[src_x, src_y, src_z] = source_xyz(bf.steer_r, steer_az, steer_el);

%% Default frequency domain weights
W = zeros(N_half, bf.num_filters);

%% Coherence matrix, diffuse field
% Equation 2.11
Gamma_vv = zeros(N_half, bf.num_filters, bf.num_filters);
for n=1:bf.num_filters
    for m=1:bf.num_filters
        % Equation 2.17
        lnm = sqrt( (bf.mic_x(n) - bf.mic_x(m))^2 ...
            +(bf.mic_y(n) - bf.mic_y(m))^2 ...
            +(bf.mic_z(n) - bf.mic_z(m))^2);
            % singular value decomposition
            [U,S,V] = svd(sinc(2*pi*f*lnm/bf.c), 'econ');
            Gamma_vv(:,n,m) = U*S*V';
    end
end

%% Delays from source to each mic
dt = delay_from_source(bf, src_x, src_y, src_z);
dt = dt-min(dt);

%% Create array vector
tau0 = zeros(n_phi, bf.num_filters);
A = zeros(N_half, n_phi, bf.num_filters);
d = zeros(N_half, bf.num_filters);
for n=1:bf.num_filters
	% Equation 2.27
	d(:,n) = exp(-j*2*pi*f*dt(n)); % Delays to steer direction
	for ip = 1:n_phi;
		phi = phi_rad(ip);
		x_phi = bf.steer_r*cos(phi)*cos(steer_el);
		y_phi = bf.steer_r*sin(phi)*cos(steer_el);
		z_phi = bf.steer_r*sin(steer_el);
		tau0(ip, n) = sqrt((x_phi-bf.mic_x(n))^2 ...
				   + (y_phi-bf.mic_y(n))^2 ...
				   + (z_phi-bf.mic_z(n))^2)/bf.c;
	end
end
tau = tau0-min(min(tau0));

for n=1:bf.num_filters
	for ip = 1:n_phi
		% N_half x n_phi x Nm
		A(:, ip, n) = exp(-j*2*pi*f*tau(ip, n));
	end
end

switch lower(bf.type)
	case 'sdb'
		% Superdirective
		for iw = 1:N_half
			% Equation 2.33
			I = eye(bf.num_filters, bf.num_filters);
			d_w = d(iw,:).';
			Gamma_vv_w = squeeze(Gamma_vv(iw, :, :));
			Gamma_vv_w_diagload = Gamma_vv_w + mu(iw)*I;
			Gamma_vv_w_inv = inv(Gamma_vv_w_diagload);
			num = Gamma_vv_w_inv * d_w;
			denom1 = d_w' * Gamma_vv_w_inv;
			denom2 = denom1 * d_w;
			W_w = num / denom2;
			W(iw, :) = W_w.';
		end
	case 'dsb'
		% Delay and sum
		for iw = 1:N_half
			% Equation 2.31
			% W = 1/N*d
			d_w = d(iw, :);
			W_w = 1/bf.num_filters * d_w;
			W(iw, :) = W_w.';
		end
	otherwise
		error('Invalid type, use SDB or DSB');
end

%% Convert w to time domain
W_full = zeros(N, bf.num_filters);
W_full = W(1:N_half, :);
for i=N_half+1:N
	W_full(i,:) = conj(W(N_half-(i-N_half),:));
end
bf.w = zeros(bf.fir_length, bf.num_filters);
w_tmp = zeros(N, bf.num_filters);
idx_max = zeros(bf.num_filters, 1);
for i=1:bf.num_filters
	h = real(fftshift(ifft(W_full(:,i))));
	w_tmp(:,i) = h;
	idx_max(i) = find(h == max(h));
end

center_idx = round(mean(idx_max));
start = center_idx - floor(bf.fir_length/2);
switch lower(bf.type_filt)
    case 'hann'
        win0 = hann (bf.fir_length);
    case 'hamming'
        win0 = hamming(bf.fir_length);
    case 'taylorwin'
        win0 = taylorwin(bf.fir_length, bf.taylorwin_nbar, bf.taylorwin_sidelobe);
    case 'chebwin'
        win0 = chebwin(bf.fir_length, bf.chebwin_sidelobe);
    case 'kaiser'
        win0 = kaiser(bf.fir_length,bf.kaiser_beta);
end
for i=1:bf.num_filters
	win = zeros(bf.fir_length, 1);
	win_shift = center_idx - idx_max(i) - 1;
	if (win_shift >= 0)
		win(1:end-win_shift) = win0(win_shift+1:end);
	else
		win(-win_shift:end) = win0(1:end+win_shift+1);
	end
	bf.w(:,i) = w_tmp(start:start + bf.fir_length - 1, i) .* win;
end

%% Back to frequency domain to check spatial response
W2_full = zeros(N, bf.num_filters);
for i=1:bf.num_filters
	% Zero pad
	h2 = zeros(1,N);
	h2(start:start + bf.fir_length - 1) = bf.w(:,i);
	W2_full(:,i) = fft(h2);
end
W2 = W2_full(1:N_half, :);
B2 = zeros(N_half, n_phi);
for iw = 1:N_half
    WT = (W2(iw,:)').';
    AS = squeeze(A(iw,:,:)).';
    WA = WT * AS;
    B2(iw,:) = WA;
end
bf.resp_fa = B2';
bf.resp_angle = phi_rad * 180/pi;

%% Directivity in diffuse field
% Equation 2.18
% DI(exp(j Omega) = 10*log10( abs(W^H d)^2 / (W^H Gamma_vv W))
bf.f = f;
bf.di_db = zeros(1, N_half);
for iw = 1:N_half
    W_w = W2(iw,:).';
    d_w = d(iw,:).';
    Gamma_vv_w = squeeze(Gamma_vv(iw,:,:));
    W_wh = W_w';
    num = abs(W_wh * d_w)^2;
    denom1 = W_wh * Gamma_vv_w;
    denom2 = denom1 * W_w;
    di = num / denom2;
    bf.di_db(iw) = 10*log10(abs(di));
end


%% White noise gain
for iw = 1:N_half
    % WNG = abs(^w^H d)^2/(w^H w);
    W_w = W2(iw,:).';
    d_w = d(iw,:).';
    W_wh = W_w';
    num = abs(W_wh * d_w)^2;
    denom = W_wh * W_w;
    wng = num / denom2;
    wng_db(iw) = 10*log10(abs(wng));
end
bf.wng_db = wng_db;

if bf.do_plots
	%% Array
	fh(1) = figure(bf.fn);
	plot3(bf.mic_x(1), bf.mic_y(1), bf.mic_z(1), 'ro');
	hold on;
	plot3(bf.mic_x(2:end), bf.mic_y(2:end), bf.mic_z(2:end), 'bo');
	plot3(src_x, src_y, src_z, 'gx');
	plot3([0 src_x],[0 src_y],[0 src_z],'c--')
	for n=1:bf.num_filters
		text(bf.mic_x(n),  bf.mic_y(n),  bf.mic_z(n) + 20e-3, ...
		     num2str(n));
	end
	hold off
	pb2 = bf.plot_box / 2;
	axis([-pb2 pb2 -pb2 pb2 -pb2 pb2]);
	axis('square');
	grid on;
	xlabel('x (m)'); ylabel('y (m)'); zlabel('z (m)');
	view(-50, 30);
	title(['Geometry ' bf.array_id], 'Interpreter','none');

	%% Coef
	fh(2) = figure(bf.fn + 1);
	plot(bf.w)
	grid on;
	xlabel('FIR coefficient'); ylabel('Tap value');
	title(['FIR filters ' bf.array_id], 'Interpreter','none');
	ch_legend(bf.num_filters);

	%% Frequency responses
	fh(3) = figure(bf.fn + 2);
	f = linspace(0, bf.fs/2, 256);
	h = zeros(256, bf.num_filters);
	for i = 1:bf.num_filters
		h(:,i) = freqz(bf.w(:,i), 1, f, bf.fs);
	end
	plot(f, 20*log10(abs(h)));
	grid on
	ylabel('Magnitude (dB)');
	xlabel('Frequency (Hz)');
	title(['FIR magnitude responses ' bf.array_id], 'Interpreter','none');
	ch_legend(bf.num_filters);

	%% Group delays
	fh(4) = figure(bf.fn + 3);
	gd = zeros(256, bf.num_filters);
	for i = 1:bf.num_filters
		gd(:,i) = grpdelay(bf.w(:,i), 1, f, bf.fs);
	end
	plot(f, gd/bf.fs*1e6);
	grid on
	ylabel('Group delay (us)');
	xlabel('Frequency (Hz)');
	title(['FIR group delays ' bf.array_id], 'Interpreter','none');
	ch_legend(bf.num_filters);

	%% DI
	bf.fh(5) = figure(bf.fn + 4);
	semilogx(bf.f(2:end), bf.di_db(2:end))
	xlabel('Frequency (Hz)'); ylabel('DI (dB)'); grid on;
	legend('Suppression of diffuse field noise','Location','SouthEast');
	title(['Directivity Index ' bf.array_id], 'Interpreter','none');

	%% WNG
	bf.fh(6) = figure(bf.fn + 5);
	semilogx(bf.f(2:end), bf.wng_db(2:end))
	xlabel('Frequency (Hz)'); ylabel('WNG (dB)'); grid on;
	legend('Attenuation of uncorrelated noise','Location','SouthEast');
	title(['White noise gain ' bf.array_id], 'Interpreter','none');
	drawnow;

	%% 2D
	bf.fh(7) = figure(bf.fn + 6);
	colormap(jet);
	phi_deg = phi_rad*180/pi;
	imagesc(bf.f, bf.resp_angle, 20*log10(abs(bf.resp_fa)), [-30 0]);
	set(gca,'YDir','normal')
	grid on;
	colorbar;
	xlabel('Frequency (Hz)'); ylabel('Angle (deg)');
	title(['Spatial response ' bf.array_id], 'Interpreter','none');

	%% Polar
	bf.fh(8) = figure(bf.fn + 7);
	flist = [1000 2000 3000 4000];
	idx = [];
	for i = 1:length(flist)
		idx(i) = find(f > flist(i), 1, 'first');
	end
	bf.resp_polar = abs(B2(idx,:));
	if exist('OCTAVE_VERSION', 'builtin')
		polar(phi_rad, bf.resp_polar);
	else
		polarplot(phi_rad, bf.resp_polar);
	end
	legend('1 kHz','2 kHz','3 kHz','4 kHz');
	title(['Polar response ' bf.array_id], 'Interpreter','none');
end

%% Create data for simulation 1s per angle
if bf.create_simulation_data
	if isempty(bf.sinerot_fn)
		fprintf(1, 'No file for 360 degree sine source rotate specified\n');
	else
		rotate_sound_source(bf, bf.sinerot_fn, 'sine');
	end

	if isempty(bf.noiserot_fn)
		fprintf(1, 'No file for 360 degree random noise source rotate specified\n');
	else
		rotate_sound_source(bf, bf.noiserot_fn, 'noise');
	end


	if isempty(bf.diffuse_fn)
		fprintf(1, 'No file for diffuse noise field specified\n');
	else
		fprintf(1, 'Creating diffuse noise field...\n');
		fsi = 384e3; % Target interpolated rate
		p = round(fsi / bf.fs); % Interpolation factor
		fsi = p * bf.fs; % Recalculate high rate
		ti = 1/fsi;  % period at higher rate
		t_add = 10.0/bf.c; % Additional signal time for max 20m propagation
		t0 = bf.diffuse_t + t_add; % Total sine length per angle
		n0 = floor(bf.fs * t0);
		nt = floor(bf.fs * bf.diffuse_t); % Number samples output per angle
		nti = p * nt; % Number samples output per angle at high rate
		el = 0;
		for az_deg = -160:20:180 % Azimuth plane only noise with sources
			az = az_deg * pi/180;
			[nx, ny, nz] = source_xyz(bf.steer_r, az, el);
			dt = delay_from_source(bf, nx, ny, nz);
			dn = round(dt / ti);
			ns = rand(n0, 1) + rand(n0, 1) - 1;
			nsi = interp(ns, p);
			nmi = zeros(nti, bf.mic_n);
			for j = 1:bf.mic_n
				nmi(:,j) = nmi(:,j) + nsi(end-dn(j)-nti+1:end-dn(j));
			end
		end
		nm = nmi(1:p:end, :);
		nlev = level_dbfs(nm(:,1));
		nm = nm * 10^((bf.diffuse_lev - nlev)/20);
		myaudiowrite(bf.diffuse_fn, nm, bf.fs);
	end

	if isempty(bf.random_fn)
		fprintf(1, 'No file for random noise specified\n');
	else
		fprintf(1, 'Creating random noise ...\n');
		nt = bf.fs * bf.random_t;
		rn = rand(nt, bf.num_filters) + rand(nt, bf.num_filters) - 1;

		nlev = level_dbfs(rn(:,1));
		rn = rn * 10^((bf.random_lev - nlev)/20);
		myaudiowrite(bf.random_fn, rn, bf.fs);
	end

	if isempty(bf.mat_fn)
		fprintf(1, 'No file for beam pattern simulation data specified.\n');
	else
		fprintf(1, 'Saving design to %s\n', bf.mat_fn);
		bf_copy = bf;
		bf.fh = []; % Don't save the large figures, this avoids a warning print too
		mkdir_check(bf.data_path);
		save(bf.mat_fn, 'bf');
		bf = bf_copy;
	end
end

fprintf(1, 'Done.\n');

end

%% Helper functions

function rotate_sound_source(bf, fn, type);
	fprintf(1, 'Creating 360 degree %s source rotate...\n', type);
	fsi = 384e3; % Target interpolated rate
	t_add = 10.0/bf.c; % Additional signal time for max 10m propagation
	t_ramp = 5e-3; % 5 ms ramp to start and end of each angle tone
	t_idle = 25e-3; % 25 ms idle to start and end of each angle tone
	p = round(fsi / bf.fs); % Interpolation factor
	fsi = p * bf.fs; % Recalculate high rate
	ti = 1/fsi;  % perid at higher rate
	tt0 = bf.sinerot_t + t_add; % Total sine length per angle
	nt = bf.fs * bf.sinerot_t; % Number samples output per angle
	nt0 = floor(bf.fs * tt0); % Number samples output per angle
	nti = p * nt; % Number samples output per angle at high rate
	n_ramp = round(t_ramp * fsi);
	n_idle = round(t_idle * fsi);
	n_win = round(tt0 * fsi);
	win = ones(n_win, 1); % Make a kind of hat shape window
	win(n_idle:(n_idle + n_ramp - 1)) = linspace(0, 1, n_ramp);
	win((end - n_idle - n_ramp + 1):(end - n_idle)) = linspace(1, 0, n_ramp);
	win(1:n_idle) = zeros(1, n_idle);
	win(end - n_idle + 1: end) = zeros(1, n_idle);
	switch lower(type)
		case 'sine'
			si = multitone(fsi, bf.sinerot_f, bf.sinerot_a, tt0);
			si = si .* win;
		case 'noise'
			[b, a] = butter(4, 2*[100 7000]/bf.fs);
			ns0 = bf.sinerot_a * (2 * rand(round(tt0*bf.fs) + 1, 1) - 1);
			nsf = filter(b, a, ns0);
			si = interp(nsf, p);
			si = si(1:n_win) .* win;
		otherwise
			error('no test signal type defined');
	end
	test_az = (bf.sinerot_az_start:bf.sinerot_az_step:bf.sinerot_az_stop) * pi/180;
	test_n = length(test_az);
	test_el = zeros(1, test_n);
	[test_x, test_y, test_z] = source_xyz(bf.steer_r, test_az, test_el);
	td = zeros(test_n * nt, bf.mic_n);
	for i = 1:length(test_az)
		dt = delay_from_source(bf, test_x(i), test_y(i), test_z(i));
		dn = round(dt / ti);
		mi = zeros(nti, bf.num_filters);
		for j = 1:bf.mic_n
			mi(:,j) = mi(:,j) + si(end-dn(j)-nti+1:end-dn(j));
		end
		i1 = (i - 1) * nt + 1;
		i2 = i1 + nt -1;
		for j = 1:bf.mic_n
			m = mi(1:p:end, j);
			td(i1:i2, j) = m;
		end
	end
	myaudiowrite(fn, td, bf.fs);
end

function [x, y, z] = source_xyz(r, az, el)

x = r * cos(az) .* cos(el);
y = r * sin(az) .* cos(el);
z = r * sin(el);

end

function dt = delay_from_source(bf, src_x, src_y, src_z)

dm = zeros(1,bf.num_filters);
for n=1:bf.num_filters
	dm(n) = sqrt((src_x - bf.mic_x(n))^2 ...
		     + (src_y - bf.mic_y(n))^2 ...
		     + (src_z - bf.mic_z(n))^2);
end
dt = dm/bf.c;

end

function ch_legend(n)
switch n
	case 2
		legend('1', '2');
	case 3
		legend('1', '2', '3');
	case 4
		legend('1', '2', '3', '4');
	case 5
		legend('1', '2', '3', '4', '5');
	case 6
		legend('1', '2', '3', '4', '5', '6');
	case 7
		legend('1', '2', '3', '4', '5', '6', '7');
	otherwise
		legend('1', '2', '3', '4', '5', '6', '7', '8');
end
end

function myaudiowrite(fn, x, fs)
[~, ~, ext] = fileparts(fn);
if strcmpi(ext, '.raw')
	s = size(x);
	xq = zeros(s(1) * s(2), 1, 'int16');
	scale = 2^15;
	xs = round(x * scale);
	xs(xs > scale - 1) = scale -1;
	xs(xs < -scale) = -scale;
	for i = 1:s(2)
		xq(i:s(2):end) = xs(:,i);
	end
	fh = fopen(fn, 'wb');
	fwrite(fh, xq, 'int16');
	fclose(fh);
else
	audiowrite(fn, x, fs);
end
end
