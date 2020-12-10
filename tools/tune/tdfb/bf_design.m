% bf = bf_design(bf)
%
% This script calculates beamformer filters with superdirective design
% criteria.

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2020, Intel Corporation. All rights reserved.
%
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

function bf = bf_design(bf)

addpath('../../test/audio/test_utils');
addpath('../../test/audio/std_utils');
mkdir_check('plots');
mkdir_check('data');

if length(bf.steer_az) ~= length(bf.steer_el)
	error('The steer_az and steer_el lengths need to be equal.');
end

if length(find(bf.steer_az > 180)) || length(find(bf.steer_az < -180))
	error('The steer_az angles need to be -180 to +180 degrees');
end

if length(find(bf.steer_el > 90)) || length(find(bf.steer_el < -90))
	error('The steer_el angles need to be -90 to +90 degrees');
end

if isempty(bf.num_filters)
	if isempty(bf.input_channel_select)
		bf.num_filters = bf.mic_n;
	else
		bf.num_filters = length(bf.input_channel_select);
	end
end

switch lower(bf.array)
	case 'line'
		bf = bf_array_line(bf);
	case 'circular'
		bf = bf_array_circ(bf);
	case 'rectangle'
		bf = bf_array_rect(bf);
	case 'lshape'
		bf = bf_array_lshape(bf);
	case 'xyz'
		bf = bf_array_xyz(bf);
	otherwise
		error('Invalid array type')
end

bf = bf_array_rot(bf);

% The design function handles only single (az, el) value, so need to
% loop every steer angle.
bf.num_angles = length(bf.steer_az);
all_az = bf.steer_az;
all_el = bf.steer_el;
all_array_id = bf.array_id;
all_sinerot_fn = bf.sinerot_fn;
all_diffuse_fn = bf.diffuse_fn;
all_random_fn = bf.random_fn;
all_mat_fn = bf.mat_fn;

for n = 1:bf.num_angles
	bf.steer_az = all_az(n);
	bf.steer_el = all_el(n);
	bf.array_id = all_array_id{n};
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
bf.sinerot_fn = all_sinerot_fn;
bf.diffuse_fn = all_diffuse_fn;
bf.random_fn = all_random_fn;
bf.mat_fn = all_mat_fn;
bf.w = w_all;

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
            Gamma_vv(:,n,m) = sinc(2*pi*f*lnm/bf.c);
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

%% Superdirective
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

%% Convert w to time domain
W_full = zeros(N, bf.num_filters);
W_full = W(1:N_half, :);
for i=N_half+1:N
	W_full(i,:) = conj(W(N_half-(i-N_half),:));
end
skip = floor((N - bf.fir_length)/2);
win = kaiser(bf.fir_length,bf.fir_beta);
bf.w = zeros(bf.fir_length, bf.num_filters);
for i=1:bf.num_filters
	w_tmp = real(fftshift(ifft(W_full(:,i))));
	bf.w(:,i) = w_tmp(skip + 1:skip + bf.fir_length) .* win;
end

%% Back to frequency domain to check spatial response
W2_full = zeros(N, bf.num_filters);
for i=1:bf.num_filters
	% Zero pad
	h2 = zeros(1,N);
	h2(skip + 1:skip + bf.fir_length) = bf.w(:,i);
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

	%% DI
	fh(3) = figure(bf.fn + 2);
	semilogx(bf.f(2:end), bf.di_db(2:end))
	xlabel('Frequency (Hz)'); ylabel('DI (dB)'); grid on;
	legend('Suppression of diffuse field noise','Location','SouthEast');
	title(['Directivity Index ' bf.array_id], 'Interpreter','none');

	%% WNG
	fh(4) = figure(bf.fn + 3);
	semilogx(bf.f(2:end), bf.wng_db(2:end))
	xlabel('Frequency (Hz)'); ylabel('WNG (dB)'); grid on;
	legend('Attenuation of uncorrelated noise','Location','SouthEast');
	title(['White noise gain ' bf.array_id], 'Interpreter','none');
	drawnow;

	%% 2D
	fh(5) = figure(bf.fn + 4);
	colormap(jet);
	phi_deg = phi_rad*180/pi;
	imagesc(bf.f, bf.resp_angle, 20*log10(abs(bf.resp_fa)), [-30 0]);
	set(gca,'YDir','normal')
	grid on;
	colorbar;
	xlabel('Frequency (Hz)'); ylabel('Angle (deg)');
	title(['Spatial response ' bf.array_id], 'Interpreter','none');

	%% Polar
	fh(6) = figure(bf.fn + 5);
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

if isempty(bf.sinerot_fn)
	fprintf(1, 'No file for 360 degree sine source rotate specified\n');
else
	fprintf(1, 'Creating 360 degree sine source rotate...\n');
	fsi = 384e3; % Target interpolated rate
	p = round(fsi / bf.fs); % Interpolation factor
	fsi = p * bf.fs; % Recalculate high rate
	ti = 1/fsi;  % perid at higher rate
	t_add = 10.0/bf.c; % Additional signal time for max 10m propagation
	tt0 = bf.sinerot_t + t_add; % Total sine length per angle
	nt = bf.fs * bf.sinerot_t; % Number samples output per angle
	nti = p * nt; % Number samples output per angle at high rate
	t_ramp = 20e-3; % 20 ms ramps to start and end of each angle tone
	n_ramp = t_ramp * bf.fs;
	win = ones(nt, 1);
	win(1:n_ramp) = linspace(0, 1, n_ramp);
	win(end-n_ramp+1:end) = linspace(1, 0, n_ramp);
	si = multitone(fsi, bf.sinerot_f, bf.sinerot_a, tt0);
	test_az = (bf.sinerot_az_start:bf.sinerot_az_step:bf.sinerot_az_stop) * pi/180;
	test_n = length(test_az);
	test_el = zeros(1, test_n);
	[test_x, test_y, test_z] = source_xyz(bf.steer_r, test_az, test_el);
	td = zeros(test_n * nt, bf.num_filters);
	for i = 1:length(test_az)
		dt = delay_from_source(bf, test_x(i), test_y(i), test_z(i));
		dn = round(dt / ti);
		mi = zeros(nti, bf.num_filters);
		for j = 1:bf.num_filters
			mi(:,j) = mi(:,j) + si(dn(j):dn(j) + nti -1);
		end
		i1 = (i - 1) * nt + 1;
		i2 = i1 + nt -1;
		for j = 1:bf.num_filters
			m = mi(1:p:end, j) .* win;
			td(i1:i2, j) = m;
		end
	end
	myaudiowrite(bf.sinerot_fn, td, bf.fs);
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
		nmi = zeros(nti, bf.num_filters);
		for j = 1:bf.num_filters
			nmi(:,j) = nmi(:,j) + nsi(dn(j):dn(j) + nti -1);
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
	mkdir_check(bf.data_path);
	save(bf.mat_fn, 'bf');
end

fprintf(1, 'Done.\n');

end

%% Helper functions

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

function myaudiowrite(fn, x, fs)

[~, ~, ext] = fileparts(fn);
if strcmp(lower(ext), '.raw')
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
