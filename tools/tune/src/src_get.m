function src = src_get(cnv)

% src_get - calculate coefficients for a src
%
% src = src_get(cnv);
%
% cnv - src parameters
% src - src result
%

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

use_firpm = 0;
use_remez = 0;
use_kaiser = 0;
switch lower(cnv.design)
	case 'firpm'
		if exist('OCTAVE_VERSION', 'builtin')
			use_remez = 1;
		else
			use_firpm = 1;
		end
	case 'kaiser'
		use_kaiser = 1;
	otherwise
		error('Unknown FIR design request!');
end

if abs(cnv.fs1-cnv.fs2) < 1
        %% Return minimum needed for scripts to work
        src.L=1;
        src.M=1;
        src.odm=1;
        src.idm=1;
        src.MOPS=0;
        src.c_pb = 0;
        src.c_sb = 0;
        src.fir_delay_size = 0;
        src.out_delay_size = 0;
        src.blk_in = 1;
        src.blk_out = 1;
        src.gain = 1;
	src.filter_length = 0;
        return
end

%% fractional SRC parameters
[L, M] = src_factor1_lm(cnv.fs1, cnv.fs2);
src.L = L;
src.M = M;
src.num_of_subfilters = L;
[src.idm src.odm] = src_find_l0m0(src.L, src.M);
min_fs = min(cnv.fs1,cnv.fs2);
src.f_pb = min_fs*cnv.c_pb;
src.f_sb = min_fs*cnv.c_sb;
src.c_pb = cnv.c_pb;
src.c_sb = cnv.c_sb;
src.fs1 = cnv.fs1;
src.fs2 = cnv.fs2;
src.rp = cnv.rp;
src.rs = cnv.rs;

%% FIR PM design
fs3 = src.L*cnv.fs1;
f = [src.f_pb src.f_sb];
a = [1 0];

% Kaiser fir is not equiripple, can allow more ripple in the passband
% lower freq.
dev = [(10^(cnv.rp/20)-1)/(10^(cnv.rp/20)+1) 10^(-cnv.rs/20)];
[kn0, kw, kbeta, kftype] = kaiserord(f, a, dev, fs3);
if use_firpm
        %dev = [(10^(cnv.rp/20)-1)/(10^(cnv.rp/20)+1) 10^(-cnv.rs/20)];
        [n0,fo,ao,w] = firpmord(f,a,dev,fs3);
	n0 = round(n0*0.95); % Decrease order to 95%
end
if use_remez
        n0 = round(kn0*0.60); % Decrease order to 70%
	fo = [0 2*f/fs3 1];
	ao = [1 1 0 0];
	w =  [1 dev(1)/dev(2)];
end
if use_kaiser
	n0 = round(kn0*0.70); % Decrease order to 70%
end

% Constrain filter length to be a suitable multiple. Multiple of
% interpolation factor ensures that all subfilters are equal length.
% Make each sub-filter order multiple of N helps in making efficient
% implementation.
nfir_increment = src.num_of_subfilters * cnv.filter_length_mult;

nsf0 = (n0+1)/nfir_increment;
if nsf0 > floor(nsf0)
        n = (floor(nsf0)+1)*nfir_increment-1;
else
        n = n0;
end

src.subfilter_length = (n+1)/src.num_of_subfilters;
src.filter_length = n+1;
nfir = n;
f_sb = linspace(src.f_sb, fs3/2, 500);
stopband_ok = 0;
need_to_stop = 0;
delta = 100;
n_worse = 0;
n_worse_max = 20;
n = 1;
n_max = 100;
dn = ones(1, n_max)*1000;
fn = zeros(1, n_max);
while (stopband_ok) == 0 && (n < n_max)
        if use_firpm || use_remez
                if nfir > 1800
                        b = fir1(nfir, kw, kftype, kaiser(nfir+1, kbeta));
                else
			if use_firpm
				b = firpm(nfir,fo,ao,w);
			else
				b = remez(nfir,fo,ao,w);
			end
                end
        else
                b = fir1(nfir, kw, kftype, kaiser(nfir+1, kbeta));
        end
	m_b = max(abs(b));
	%sref = 2^(cnv.coef_bits-1);
	%bq = round(b*sref/m_b)*m_b/sref;
        bq = b;
        h_sb = freqz(bq, 1, f_sb, fs3);
        m_sb = 20*log10(abs(h_sb));
	delta_prev = delta;
        delta = cnv.rs+max(m_sb);
        fprintf('Step=%3d, Delta=%f dB, N=%d\n', n, delta, nfir);
        dn(n) = delta;
        fn(n) = nfir;
        if delta < 0
		stopband_ok = 1;
        else
		if delta_prev < delta
                        n_worse = n_worse+1;
                        if n_worse > n_worse_max
                                need_to_stop = 1; % No improvement, reverse
                                idx = find(dn == min(dn), 1, 'first');
                                nfir = fn(idx);
                        else
				nfir = nfir + nfir_increment;
                        end
		else
			if need_to_stop == 0
				nfir = nfir + nfir_increment;
			else
				stopband_ok = 1; % Exit after reverse
				fprintf('Warning: Filter stop band could not be ');
				fprintf('reached.\n', cnv.coef_bits);
			end
		end
        end
        n = n + 1;
end

f_p = linspace(0, fs3/2, 2000);
m_db = 20*log10(abs(freqz(bq, 1, f_p, fs3)));
i_pb = find(f_p < src.f_pb);
i_m3 = find(m_db > -3);
f_m3 = f_p(i_m3(end));
g_dc = m_db(1);
g_dc_comp_lin = 10^(-g_dc/20);

if 1
        p_ymin = floor((-cnv.rs-50)/10)*10;
        p_ymax = 10;
        figure(1);
        clf;
        plot(f_p, m_db);
        grid on;
        xlabel('Frequency (Hz)');
        ylabel('Magnitude (dB)');
        axis([0 fs3/2 p_ymin p_ymax]);
        hold on
        plot([src.f_sb src.f_sb],[p_ymin p_ymax], 'r--');
        plot([0 fs3],[-cnv.rs -cnv.rs], 'r--');
        hold off
	axes('Position', [ 0.55 0.5 0.35 0.4]);
        box on;
        plot([0 src.f_pb], +0.5*cnv.rp*ones(1,2), 'r--');
        hold on
        plot([0 src.f_pb], -0.5*cnv.rp*ones(1,2), 'r--');
        plot(f_p(i_m3), m_db(i_m3));
        axis([0 f_m3 -3 0.5]);
        hold off
        grid on;
        box off;
end

src.subfilter_length = ceil((nfir+1)/src.num_of_subfilters);
src.filter_length = src.subfilter_length*src.num_of_subfilters;
src.b = zeros(src.filter_length,1);
src.gain = 1;
src.b(1:nfir+1) = b * src.L * g_dc_comp_lin * 10^(cnv.gain/20);
m = max(abs(src.b));
gmax = (32767/32768)/m;
maxshift = floor(log(gmax)/log(2));
src.b = src.b * 2^maxshift;
src.gain = 1/2^maxshift;
src.shift = maxshift;

%% Reorder coefficients
if 1
        src.coefs = src.b;
else
        src.coefs = zeros(src.filter_length,1);
        for n = 1:src.num_of_subfilters
                i11 = (n-1)*src.subfilter_length+1;
                i12 = i11+src.subfilter_length-1;
                src.coefs(i11:i12) = src.b(n:src.num_of_subfilters:end);
        end
end

src.halfband = 0;
src.blk_in = M;
src.blk_out = L;
src.MOPS = cnv.fs1/M*src.filter_length/1e6;
src.fir_delay_size = src.subfilter_length + ...
        (src.num_of_subfilters-1)*src.idm + src.blk_in;
src.out_delay_size = (src.num_of_subfilters-1)*src.odm + 1;

end
