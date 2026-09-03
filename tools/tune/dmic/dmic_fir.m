% PDM_DECIM_FIRPM
%
% [ coef, shift, used_passhz, used_stophz, used_rs, pb_ripple, sb_ripple, pass] = ...
%        pdm_decim_firpm( rp, rs, passhz, stophz, fs_cic, fs_fir, max_length, linph)
%

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2019, Intel Corporation. All rights reserved.
%
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

function [ coef, shift, used_passhz, used_stophz, used_rs, pb_ripple, sb_ripple, pass] = ...
        dmic_fir( rp, rs, passhz, stophz, fs_cic, fs_fir, max_length, linph)


if exist('OCTAVE_VERSION', 'builtin')
	error('This function runs only in Matlab');
end

%% Suppress iteration prints
verbose = 0;
do_plots = 0;

%% cic5 response
f = linspace(1, fs_fir/2, 5000);
w = 2*pi*f/fs_cic;
j = sqrt(-1);
z = exp(j.*w);
m_cic = fs_cic / fs_fir;
g_cic = m_cic^5;
g_cic_db = 20*log10(g_cic);
z_cic_hi = z;
z_cic_lo = z.^(m_cic);

hd = (1 - z_cic_lo.^(-1)).^5;
hi = (1 ./ (1 - z_cic_hi.^(-1))).^5;
h_cic = (1/g_cic) .* hi .* hd;

%% Get firpm design spec

[n, fv, mv, w] = filter_spec(rp, rs, passhz, stophz, fs_fir, f, h_cic, linph);
if linph
        n = min(n, max_length-1);
end

%% Iterate order and passband width
iterate = 1;
lo_ind = find(f < passhz/20);
pb_ind = find(f < passhz);
sb_ind = find(f > stophz);
pb_ripple_min = 1000;
sb_ripple_min = 1000;
pb_ripple_prev = 1000;
sb_ripple_prev = 1000;
pb_worse = 0;
sb_worse = 0;

pass = 0;
nstep = 2;
cpb = 1.0;
csb = 1.0;
cpb_min = 0.95;
cpb_dec = 0.0005;
csb_inc = 0.0005;
rs_dec = 1;
passhz0 = passhz;
stophz0 = stophz;
used_passhz = passhz;
used_stophz = stophz;
used_rs = rs;
ni_max = 200;
rs_min = 90;

ni = 0;
while (iterate > 0)
        ni = ni + 1;
	if verbose
		fprintf(1, 'N=%3d, max=%3d, order=%3d, bw=%5.2f kHz, sb=%5.2f kHz, rp=%4.1f dB', ...
			ni, max_length-1, n, passhz/1e3, stophz/1e3, rs);
	end
        b = firpm(n, fv, mv, w);
        if linph == 0
		error('not supported');
        end

        % Set low frequency gain to max 0 dB
        h_lo = freqz(b, 1, f(lo_ind), fs_fir);
        g = 1/max(abs(h_lo));
        b = b * g;
        n_taps = length(b);
        n_fir = n_taps-1;

        %% Scale FIR coefficient to be close to 1.0
        max_abs_coef = max(abs(b));
        scale = 0.9999/max_abs_coef;

        % Check for amount of left shifts for coefficients / right shifts
        % for data.
        shift = floor(log(scale)/log(2));
        c = 2^shift;
        coef = b * c;


        %% Compute FIR response as H(z^M)
        z_fir = z.^m_cic;
        h_fir = b(1);
        for i=2:(n_fir+1)
                h_fir = h_fir + b(i).*z_fir.^(-i+1);
        end

        %% Combine CIC5 and FIR response
        h_decim = h_cic .* h_fir;

        %% Check passband
        % If PB is met then SB should be OK as well due to weights
        h_passband_db = 20*log10(abs(h_decim(pb_ind)));
        h_stopband_db = 20*log10(abs(h_decim(sb_ind)));
        pb_ripple = (max(h_passband_db)-min(h_passband_db))/2;
        sb_ripple = max(h_stopband_db);
	if verbose
		fprintf(1,', rp=%4.2f, rs=%5.1f', pb_ripple, sb_ripple);
	end

        if (sb_ripple < -rs) && (pb_ripple < rp)
                iterate = 0;
                pass = 1;
        else
                % Check if length can be increased or generic filter spec relaxed
                if (sb_ripple < sb_ripple_min) && (pb_ripple < pb_ripple_min)
                        sb_ripple_min = sb_ripple;
                        pb_ripple_min = pb_ripple;
                        n_min = n;
                        fv_min = fv;
                        mv_min = mv;
                        w_min = w;
			if verbose
				fprintf(' +');
			end
                end
                if (n_taps + nstep < max_length + 1) && (pb_worse < 10) && (sb_worse < 10)
                        n = n + nstep;
                else
                        cpb = cpb - cpb_dec;
                        if cpb < cpb_min
                                if passhz < 24e3
                                        cpb = cpb_min;
                                        rs = rs - rs_dec;
                                        if rs < rs_min
                                                rs = rs_min;
                                                csb = csb + csb_inc;
                                        end
                                end
                        end
                        passhz = passhz0 * cpb;
                        used_passhz = passhz;
                        stophz = stophz0 * csb;
                        used_stophz = stophz;
                        used_rs = rs;
                        [n, fv, mv, w] = filter_spec(rp, rs, passhz, stophz, fs_fir, f, h_cic, linph);
                        if linph
                                n = min(n, max_length-1);
                        end
                        pb_ind = find(f < passhz);
                        sb_ind = find(f > stophz);
                        pb_worse = 0;
                        sb_worse = 0;
			if verbose
				fprintf(1, ' *');
			end
                end
                if pb_ripple > pb_ripple_prev
                        pb_worse = pb_worse + 1;
                else
                        pb_worse = 0;
                end
                if sb_ripple > sb_ripple_prev
                        sb_worse = sb_worse + 1;
                else
                        sb_worse = 0;
                end
                if ni > ni_max
                        n = n_min;
                        fv = fv_min;
                        mv = mv_min;
                        w = w_min;
                        iterate = 0;
			if verbose
				fprintf(1, ', Iterate stop, restore n=%d', n);
			end
                end
                pb_ripple_prev = pb_ripple;
                sb_ripple_prev = sb_ripple;
	end
	if verbose
		fprintf(1, '\n');
	end

end

if do_plots
	figure;
	plot(f, 20*log10(abs(h_decim)), ...
		f, 20*log10(abs(h_fir)), '--', f, 20*log10(abs(h_cic)), '--');
	grid on;
	xlabel('Frequency (Hz)');
	ylabel('Magniture (dB)');
	legend('Combined', 'FIR', 'CIC', 'Location', 'SouthWest');
end

end

function [n, fv, mv, w] = filter_spec(rp, rs, passhz, stophz, fs_fir, f, h_cic, linph)

% Prepare for normal firpm filter design
dp0 = (10^(rp/20)-1)/(10^(rp/20)+1);
ds0 = 10^(-rs/20);
if linph
        dp = dp0;
        ds = ds0;
else
        % See https://www.cs.tut.fi/~ts/FIR_minimumphase.pdf
        den = 1 + dp0^2 - 0.5 * ds0^2;
        dp = 2 * dp0 / den;
        ds = 0.5 * ds0^2 / den;
end

n_pb = 16; % Number of tilted line slopes for passband
% Make the start of equalized region high enough to have a flat band
% from DC to some kHz. It's the 1st band for firpmord().
f_pb = linspace(passhz/20, passhz, n_pb);
% Interpolate complex CIC^5 response to this grid
h_cic_pb = interp1(f, h_cic, f_pb, 'linear');
% Inverse response
h_seq = abs(h_cic_pb(1))./abs(h_cic_pb);
if linph == 0
        h_seq = h_seq .^2;
end

% Specify response in bands for firpm() / remez()
% First band is from DC to 1st point (1 kHz)
f_fir_cic = [ 0 0.9999*f_pb(1) ];
a_fir_cic = [ 1 h_seq(1) ]; % Firpm/Remez supports slopes with two points for gain
dev_cic = [ dp ];
for n = 2:n_pb;
        f_fir_cic = [ f_fir_cic f_pb(n-1) 0.9999*f_pb(n) ];
        a_fir_cic = [ a_fir_cic h_seq(n-1) h_seq(n) ];
        dev_cic(n) = dp * h_seq(n)^2; % Ripple spec relax as square of eq
end

f_fir = f_fir_cic;
a_fir = a_fir_cic;
dev = dev_cic;

% Append transition/stop band
f_fir = [ f_fir stophz 0.5*fs_fir];
a_fir = [ a_fir 0 0 ];
dev   = [ dev ds ];

fv = 2*f_fir/fs_fir;
mv = a_fir;
a_fir_bands = 0.5*(a_fir(1:2:end)+a_fir(2:2:end));
sb = (a_fir_bands == 0); % Find stopband
devr = dev ./ (sb + a_fir_bands); % Get relative deviation
w = max(devr)./devr; % Invert and normalize

% Get FIR order estimate with firpmord()
[n, ~,  ~, ~] = firpmord([passhz stophz], [1 0], [dp ds], fs_fir );
n = round(0.95*n/2)*2;

end
