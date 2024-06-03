function eq = eq_compute( eq )

%%
% Copyright (c) 2016, Intel Corporation
% All rights reserved.
%
% Redistribution and use in source and binary forms, with or without
% modification, are permitted provided that the following conditions are met:
%   * Redistributions of source code must retain the above copyright
%     notice, this list of conditions and the following disclaimer.
%   * Redistributions in binary form must reproduce the above copyright
%     notice, this list of conditions and the following disclaimer in the
%     documentation and/or other materials provided with the distribution.
%   * Neither the name of the Intel Corporation nor the
%     names of its contributors may be used to endorse or promote products
%     derived from this software without specific prior written permission.
%
% THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
% AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
% IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
% ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
% LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
% CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
% SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
% INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
% CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
% ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
% POSSIBILITY OF SUCH DAMAGE.
%
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
%

%% Sanity checks
if eq.enable_iir == 0 && eq.enable_fir == 0
        fprintf('Warning: Nothing to do. Please enable FIR or IIR!\n');
end

%% Extrapolate response to 0..fs/2, convert to logaritmic grid, and smooth
%  with 1/N octave filter
eq = preprocess_responses(eq);

%% Define target (e.g. speaker) response as parametric filter. This could also
%  be numerical data interpolated to the grid.
if length(eq.parametric_target_response) > 0
        [eq.t_z, eq.t_p, eq.t_k] = eq_define_parametric_eq( ...
                eq.parametric_target_response, eq.fs);
        eq.t_db = eq_compute_response(eq.t_z, eq.t_p, eq.t_k, eq.f, eq.fs);
end

if isempty(eq.t_db)
        fprintf('Warning: No target response is defined.\n');
end

%% Align responses at some frequency and dB
[eq.m_db_s, offs] = eq_align(eq.f, eq.m_db_s, eq.f_align, eq.db_align);
eq.raw_m_db = eq.raw_m_db + offs;
eq.m_db = eq.m_db + offs;
eq.t_db = eq_align(eq.f, eq.t_db, eq.f_align, eq.db_align);

%% Error to equalize = target - raw response, apply 1/N octave smoothing to
%  soften the EQ shape
eq.err_db = eq.t_db - eq.m_db;
[eq.err_db_s, eq.logsmooth_noct] = logsmooth(eq.f, eq.err_db, eq.logsmooth_eq);

%% Parametric IIR EQ definition
if eq.enable_iir
        [eq.p_z, eq.p_p, eq.p_k] = eq_define_parametric_eq(eq.peq, eq.fs);
        if max(length(eq.p_z), length(eq.p_p)) > 2*eq.iir_biquads_max
                error('Maximum number of IIR biquads is exceeded');
        end
else
        [eq.p_z, eq.p_p, eq.p_k] = tf2zp(1, 1);
end
[eq.iir_eq_db, eq.iir_eq_ph, eq.iir_eq_gd] = eq_compute_response(eq.p_z, ...
        eq.p_p, eq.p_k, eq.f, eq.fs);


%% FIR EQ computation
%  Find remaining responses error ater IIR for FIR to handle
if eq.fir_compensate_iir
        eq.err2_db = eq.err_db_s-eq.iir_eq_db;
else
        eq.err2_db = eq.err_db_s;
end

if eq.enable_fir
        [eq.t_fir_db, eq.fmin_fir, eq.fmax_fir] = ...
                get_fir_target(eq.f, eq.err2_db, eq.fmin_fir, eq.fmax_fir, ...
                eq.amin_fir, eq.amax_fir, eq.logsmooth_noct, eq.fir_autoband);

        if eq.fir_minph
                eq.b_fir = compute_minph_fir(eq.f, eq.t_fir_db, ...
                        eq.fir_length, eq.fs, eq.fir_beta);
        else
                eq.b_fir = compute_linph_fir(eq.f, eq.t_fir_db, ...
                        eq.fir_length, eq.fs, eq.fir_beta);
        end
else
        eq.b_fir = 1;
        eq.tfirdb = zeros(1,length(eq.f));
end

%% Update all responses
eq = eq_compute_tot_response(eq);

%% Normalize
eq = eq_norm(eq);

end

function eq = preprocess_responses(eq)
%% Usually too narrow measurement without the lowest and highest frequencies
[f0, m0, gd0] = fix_response_dcnyquist_mult(eq.raw_f, eq.raw_m_db, ...
        eq.raw_gd_s, eq.fs);

%% Create dense logarithmic frequency grid, then average possible multiple
%  measurements
[eq.f, eq.m_db, eq.gd_s, eq.num_responses] = map_to_logfreq_mult(f0, m0, ...
        gd0, 1, eq.fs/2, eq.np_fine);

%% Smooth response with 1/N octave filter for plotting
eq.m_db_s = logsmooth(eq.f, eq.m_db, eq.logsmooth_plot);

if length(eq.target_m_db) > 0
        % Use target_m_db as dummy group delay, ignore other than magnitude
        [f0, m0, ~] = fix_response_dcnyquist_mult(eq.target_f, ...
                eq.target_m_db, [], eq.fs);
        [~, eq.t_db, ~, ~] = map_to_logfreq_mult(f0, m0, [], 1, ...
                eq.fs/2, eq.np_fine);
end
end


function [f_hz, m_db, gd_s] = fix_response_dcnyquist(f_hz0, m_db0, gd_s0, fs)
%% Append DC and Fs/2 if missing
f_hz = f_hz0;
m_db = m_db0;
gd_s = gd_s0;
if min(f_hz) >  0
        f_hz = [0 f_hz];
        m_db = [m_db(1) m_db]; % DC the same as 1st measured point
        if length(gd_s) > 0
                gd_s = [gd_s(1) gd_s]; % DC the same as 1st measured point
        end
end
if max(f_hz) <  fs/2
        f_hz = [f_hz fs/2];
        m_db = [m_db m_db(end)]; % Fs/2 the same as last measured point
        if length(gd_s) > 0
                gd_s = [gd_s gd_s(end)]; % Fs/2 the same as last measured point
        end
end
end

function [f_hz, m_db, gd_s] = fix_response_dcnyquist_mult(f_hz0, m_db0, ...
        gd_s0, fs)

if iscolumn(f_hz0)
        f_hz0 = f_hz0.';
end
if iscolumn(m_db0)
        m_db0 = m_db0.';
end
if iscolumn(gd_s0)
        gd_s0 = gd_s0.';
end

s1 = size(f_hz0);
s2 = size(m_db0);
s3 = size(gd_s0);

if s1(1) == 0
        error('Frequencies vector is empty');
end
if (s1(1) ~= s2(1))
        error('There must be equal number of frequency and magnitude data');
end
if (s1(2) ~= s2(2))
        error('There must be equal number of points in frequency, magnitude, and group delay data');
end
if sum(s3) == 0
        gd_s0 = zeros(s2(1),s2(2));
end
for i=1:s1(1)
        [f_hz(i,:), m_db(i,:), gd_s(i,:)] = fix_response_dcnyquist(...
                f_hz0(i,:), m_db0(i,:), gd_s0(i,:), fs);
end

end

function [f_hz, m_db, gd_s] = map_to_logfreq(f_hz0, m_db0, gd_s0, f1, f2, np)
%% Create logarithmic frequency vector and interpolate
f_hz = logspace(log10(f1),log10(f2), np);
m_db = interp1(f_hz0, m_db0, f_hz);
gd_s = interp1(f_hz0, gd_s0, f_hz);
m_db(end) = m_db(end-1); % Fix NaN in the end
gd_s(end) = gd_s(end-1); % Fix NaN in the end
end

function [f_hz, mm_db, mgd_s, num] = map_to_logfreq_mult(f_hz0, m_db0, ...
        gd_s0, f1, f2, np)

s1 = size(f_hz0);
s2 = size(m_db0);
s3 = size(gd_s0);
if (s1(1) ~= s2(1))
        error('There must be equal number of frequency and magnitude data sets');
end
if (s1(2) ~= s2(2))
        error('There must be equal number of points in frequency and magnitude data');
end
num = s1(1);
if sum(s3) == 0
        gd_s0 = zeros(s2(1),s2(2));
end
for i=1:num
        [f_hz, m_db(i,:), gd_s(i,:)] = map_to_logfreq(f_hz0(i,:), ...
                m_db0(i,:), gd_s0(i,:), f1, f2, np);
end

if num > 1
        mm_db = mean(m_db);
        mgd_s = mean(gd_s);
else
        mm_db = m_db;
        mgd_s = gd_s;
end
end

function [ms_db, noct] = logsmooth(f, m_db, c)

%% Create a 1/N octave smoothing filter
ind1 = find(f < 1000);
ind2 = find(f < 2000);
noct = ind2(end)-ind1(end);
n = 2*round(c*noct/2);
b_smooth = ones(1,n)*1/n;

%% Smooth the response
tmp = filter(b_smooth, 1, m_db);
ms_db = [tmp(n/2+1:end) linspace(tmp(end), m_db(end), n/2)];
ms_db(1:n) = ones(1,n)*ms_db(n);
end

function [m_db, fmin_fir, fmax_fir] = get_fir_target(fhz, err2db, fmin_fir, ...
        fmax_fir, amin_fir, amax_fir, noct, auto)


%% Find maximum in 1-6 kHz band
idx = find(fhz > 1e3, 1, 'first') - 1;
m_db = err2db - err2db(idx);
if auto
        cf = [1e3 6e3];
        ind1 = find(fhz < cf(2));
        ind2 = find(fhz(ind1) > cf(1));
        ipeak = find(m_db(ind2) == max(m_db(ind2))) + ind2(1);
        ind1 = find(fhz < cf(1));
        ind2 = find(m_db(ind1) > m_db(ipeak));
        if length(ind2) > 0
                fmin_fir = fhz(ind2(end));
        end
        ind1 = find(fhz > cf(2));
        ind2 = find(m_db(ind1) > m_db(ipeak)) + ind1(1);
        if length(ind2) > 0
                fmax_fir = fhz(ind2(1));
        end
end

%% Find FIR target response
ind1 = find(fhz < fmin_fir);
ind2 = find(fhz > fmax_fir);
p1 = ind1(end)+1;
if length(ind2) > 0
        p2 = ind2(1)-1;
else
        p2 = length(fhz);
end
m_db(ind1) = m_db(p1);
m_db(ind2) = m_db(p2);
ind = find(m_db > amax_fir);
m_db(ind) = amax_fir;
ind = find(m_db < amin_fir);
m_db(ind) = amin_fir;

%% Smooth high frequency corner with spline
nn = round(noct/8);
x = [p2-nn p2-nn+1 p2+nn-1 p2+nn];
if max(x) < length(m_db)
        y = m_db(x);
        xx = p2-nn:p2+nn;
        yy = spline(x, y, xx);
        m_db(p2-nn:p2+nn) = yy;
end

%% Smooth low frequency corner with spline
nn = round(noct/8);
x = [p1-nn p1-nn+1 p1+nn-1 p1+nn];
if min(x) > 0
        y = m_db(x);
        xx = p1-nn:p1+nn;
        yy = spline(x, y, xx);
        m_db(p1-nn:p1+nn) = yy;
end

end

function b = compute_linph_fir(f_hz, m_db, taps, fs, beta)
if nargin < 5
        beta = 4;
end
if mod(taps,2) == 0
        fprintf('Warning: Even FIR length requested.\n');
end
n_fft = 2*2^ceil(log(taps)/log(2));
n_half = n_fft/2+1;
f_fft = linspace(0, fs/2, n_half);
m_lin = 10.^(m_db/20);
if f_hz(1) > 0
        f_hz = [0 f_hz];
        m_lin = [m_lin(1) m_lin];
end
a_half = interp1(f_hz, m_lin, f_fft, 'linear');
a = [a_half conj(a_half(end-1:-1:2))];
h = real(fftshift(ifft(a)));
b0 = h(n_half-floor((taps-1)/2):n_half+floor((taps-1)/2));
win = kaiser(length(b0), beta)';
b = b0 .* win;
if length(b) < taps
        % Append to even length
        b = [b 0];
end
end

function b_fir = compute_minph_fir(f, m_db, fir_length, fs, beta)

%% Design double length H^2 FIR
n = 2*fir_length+1;
m_lin2 = (10.^(m_db/20)).^2;
m_db2 = 20*log10(m_lin2);
blin = compute_linph_fir(f, m_db2, n, fs,  beta);

%% Find zeros inside unit circle
myeps = 1e-3;
hdzeros = roots(blin);
ind1 = find( abs(hdzeros) < (1-myeps) );
minzeros = hdzeros(ind1);

%% Find double zeros at unit circle
ind2 = find( abs(hdzeros) > (1-myeps) );
outzeros = hdzeros(ind2);
ind3 = find( abs(outzeros) < (1+myeps) );
circlezeros = outzeros(ind3);

%% Get half of the unit circle zeros
if isempty(circlezeros)
        %% We are fine ...
else
        %% Eliminate double zeros
        cangle = angle(circlezeros);
        [sorted_cangle, ind] = sort(cangle);
        sorted_czeros = circlezeros(ind);
        pos = find(angle(sorted_czeros) > 0);
        neg = find(angle(sorted_czeros) < 0);
        pos_czeros = sorted_czeros(pos);
        neg_czeros = sorted_czeros(neg(end:-1:1));
        h1 = [];
        for i = 1:2:length(pos_czeros)-1
                x=mean(angle(pos_czeros(i:i+1)));
                h1 = [h1' complex(cos(x),sin(x))]';
        end
        h2 = [];
        for i = 1:2:length(neg_czeros)-1;
                x=mean(angle(neg_czeros(i:i+1)));
                h2 = [h2' complex(cos(x),sin(x))]';
        end
        halfcirclezeros = [h1' h2']';
        if length(halfcirclezeros)*2 < length(circlezeros)-0.1
                %% Replace the last zero pair
                halfcirclezeros = [halfcirclezeros' complex(-1, 0)]';
        end
        minzeros = [ minzeros' halfcirclezeros' ]';
end

%% Convert to transfer function
bmin = mypoly(minzeros);

%% Scale peak in passhz to max m_db
hmin = freqz(bmin, 1, 512, fs);
b_fir = 10^(max(m_db)/20)*bmin/max(abs(hmin));

end

function tf = mypoly( upolyroots )

% Sort roots to increasing angle to ensure more consistent behavior
aa = abs(angle(upolyroots));
[sa, ind] = sort(aa);
polyroots = upolyroots(ind);

n = length(polyroots);
n1 = 16; % do not change, hardwired to 16 code below

if n < (2*n1+1)
        % No need to split
        tf = poly(polyroots);
else
        % Split roots evenly to 16 poly computations
        % The fist polys will rpb+1 roots and the rest
        % rpb roots to compute
        rpb = floor(n/n1);
        rem = mod(n,n1);
        i1 = zeros(1,n1);
        i2 = zeros(1,n1);
        i1(1) = 1;
        for i = 1:n1-1;
                if rem > 0
                        i2(i) = i1(i)+rpb;
                        rem = rem-1;
                else
                        i2(i) = i1(i)+rpb-1;
                end
                i1(i+1) = i2(i)+1;
        end
        i2(n1) = n;
        tf101 = poly(polyroots(i1(1):i2(1)));
        tf102 = poly(polyroots(i1(2):i2(2)));
        tf103 = poly(polyroots(i1(3):i2(3)));
        tf104 = poly(polyroots(i1(4):i2(4)));
        tf105 = poly(polyroots(i1(5):i2(5)));
        tf106 = poly(polyroots(i1(6):i2(6)));
        tf107 = poly(polyroots(i1(7):i2(7)));
        tf108 = poly(polyroots(i1(8):i2(8)));
        tf109 = poly(polyroots(i1(9):i2(9)));
        tf110 = poly(polyroots(i1(10):i2(10)));
        tf111 = poly(polyroots(i1(11):i2(11)));
        tf112 = poly(polyroots(i1(12):i2(12)));
        tf113 = poly(polyroots(i1(13):i2(13)));
        tf114 = poly(polyroots(i1(14):i2(14)));
        tf115 = poly(polyroots(i1(15):i2(15)));
        tf116 = poly(polyroots(i1(16):i2(16)));
        % Combine coefficients with convolution
        tf21 = conv(tf101, tf116);
        tf22 = conv(tf102, tf115);
        tf23 = conv(tf103, tf114);
        tf24 = conv(tf104, tf113);
        tf25 = conv(tf105, tf112);
        tf26 = conv(tf106, tf111);
        tf27 = conv(tf107, tf110);
        tf28 = conv(tf108, tf109);
        tf31 = conv(tf21, tf28);
        tf32 = conv(tf22, tf27);
        tf33 = conv(tf23, tf26);
        tf34 = conv(tf24, tf25);
        tf41 = conv(tf31, tf34);
        tf42 = conv(tf32, tf33);
        tf   = conv(tf41, tf42);

        % Ensure the tf coefficients are real if rounding issues
        tf = real(tf);
end

end
