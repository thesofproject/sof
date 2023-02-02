function [z, p, k] = eq_define_parametric_eq(peq, fs)

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

% Parametric types
PEQ_HP1 = 1; PEQ_HP2 = 2; PEQ_LP1 = 3; PEQ_LP2 = 4;
PEQ_LS1 = 5; PEQ_LS2 = 6; PEQ_HS1 = 7; PEQ_HS2 = 8;
PEQ_PN2 = 9; PEQ_LP4 = 10; PEQ_HP4 = 11; PEQ_LP2G= 12;
PEQ_HP2G = 13; PEQ_BP2 = 14; PEQ_NC2 = 15; PEQ_LS2G = 16;
PEQ_HS2G = 17;

sp = size(peq);
z = [];
p = [];
k = 1;
for i=1:sp(1)
        type = peq(i,1);
        f = peq(i,2);
        g = peq(i,3);
        Q = peq(i,4);
        if f < fs/2
                a0 = [];
                b0 = [];
                z0 = [];
                p0 = [];
                k0 = [];
                switch peq(i,1)
                        case PEQ_HP1, [z0, p0, k0] = butter(1, 2*f/fs, 'high');
                        case PEQ_HP2, [z0, p0, k0] = butter(2, 2*f/fs, 'high');
                        case PEQ_HP4, [z0, p0, k0] = butter(4, 2*f/fs, 'high');
                        case PEQ_LP1, [z0, p0, k0] = butter(1, 2*f/fs);
                        case PEQ_LP2, [z0, p0, k0] = butter(2, 2*f/fs);
                        case PEQ_LP4, [z0, p0, k0] = butter(4, 2*f/fs);
                        case PEQ_LS1, [b0, a0] = low_shelf_1st(f, g, fs);
                        case PEQ_LS2, [b0, a0] = low_shelf_2nd(f, g, fs);
                        case PEQ_HS1, [b0, a0] = high_shelf_1st(f, g, fs);
                        case PEQ_HS2, [b0, a0] = high_shelf_2nd(f, g, fs);
                        case PEQ_PN2, [b0, a0] = peak_2nd(f, g, Q, fs);
                        case PEQ_HP2G, [b0, a0] = high_pass_2nd_reasonance(f, Q, fs);
                        case PEQ_LP2G, [b0, a0] = low_pass_2nd_reasonance(f, Q, fs);
                        case PEQ_BP2, [b0, a0] = band_pass_2nd(f, Q, fs);
                        case PEQ_NC2, [b0, a0] = notch_2nd(f, Q, fs);
                        case PEQ_LS2G, [b0, a0] = low_shelf_2nd_google(f, g, fs);
                        case PEQ_HS2G, [b0, a0] = high_shelf_2nd_google(f, g, fs);
                        otherwise
                                error('Unknown parametric EQ type');
                end
                if ~isempty(a0)
                        [z0, p0, k0] = tf2zp(b0, a0);
                end
                if ~isempty(k0)
                        z = [z ; z0(:)];
                        p = [p ; p0(:)];
                        k = k * k0;
                end
        end
end
end

function [b, a] = low_shelf_1st(fhz, gdb, fs)
zw = 2*pi*fhz;
w = wmap(zw, fs);
glin = 10^(gdb/20);
bs = [1 glin*w];
as = [1 w];
[b, a] = my_bilinear(bs, as, fs);
end

function [b, a] = low_shelf_2nd(fhz, gdb, fs)
zw = 2*pi*fhz;
w = wmap(zw, fs);
glin = 10^(gdb/20);
bs = [1 w*sqrt(2*glin) glin*w^2];
as = [1 w*sqrt(2) w^2];
[b, a] = my_bilinear(bs, as, fs);
end

function [b, a] = high_shelf_1st(fhz, gdb, fs)
zw = 2*pi*fhz;
w = wmap(zw, fs);
glin = 10^(gdb/20);
bs = [glin w];
as = [1 w];
[b, a] = my_bilinear(bs, as, fs);
end

function [b, a] = high_shelf_2nd(fhz, gdb, fs)
zw = 2*pi*fhz;
w = wmap(zw, fs);
glin = 10^(gdb/20);
bs = [glin w*sqrt(2*glin) w^2];
as = [1 w*sqrt(2) w^2];
[b, a] = my_bilinear(bs, as, fs);
end


function [b, a] = peak_2nd(fhz, gdb, Q, fs)
	% Reference http://www.musicdsp.org/files/Audio-EQ-Cookbook.txt
	A = 10^(gdb/40); % Square root of linear gain
	wc = 2*pi*fhz/fs;

	if Q <= 0
		% To fix gui edge cases, comment from CRAS code:
		% When Q = 0, the above formulas have problems. If we
		% look at the z-transform, we can see that the limit
		% as Q->0 is A^2, so set the filter that way.
		b = [A * A, 0, 0];
		a = [1, 0, 0];
		return;
	end

	alpha = sin(wc)/(2*Q);
	b0 = 1 + alpha * A;
	b1 = -2 * cos(wc);
	b2 = 1 - alpha * A;
	a0 = 1 + alpha / A;
	a1 = -2 * cos(wc);
	a2 = 1 - alpha / A;
	b = [b0 / a0 b1 / a0 b2 / a0];
	a = [1 a1 / a0 a2 / a0];
end

function [b, a] = high_pass_2nd_reasonance(f, resonance, fs)
	cutoff = f/(fs/2);
	% Limit cutoff to 0 to 1.
	cutoff = max(0.0, min(cutoff, 1.0));

	if cutoff == 1 || cutoff == 0
		% When cutoff is one, the z-transform is 0.
		% When cutoff is zero, we need to be careful because the above
		% gives a quadratic divided by the same quadratic, with poles
		% and zeros on the unit circle in the same place. When cutoff
		% is zero, the z-transform is 1.

		b = [1 - cutoff, 0, 0];
		a = [1, 0, 0];
		return;
	end

	% Compute biquad coefficients for highpass filter
	resonance = max(0.0, resonance); % can't go negative
	g = 10.0^(0.05 * resonance);
	d = sqrt((4 - sqrt(16 - 16 / (g * g))) / 2);

	theta = pi * cutoff;
	sn = 0.5 * d * sin(theta);
	beta = 0.5 * (1 - sn) / (1 + sn);
	gamma = (0.5 + beta) * cos(theta);
	alpha = 0.25 * (0.5 + beta + gamma);

	b0 = 2 * alpha;
	b1 = 2 * -2 * alpha;
	b2 = 2 * alpha;
	a1 = 2 * -gamma;
	a2 = 2 * beta;

	b = [b0, b1, b2];
	a = [1.0, a1, a2];
end

function [b, a] = low_pass_2nd_reasonance(f, resonance, fs)
	cutoff = f/(fs/2);
	% Limit cutoff to 0 to 1.
	cutoff = max(0.0, min(cutoff, 1.0));

	if cutoff == 1 || cutoff == 0
		% When cutoff is 1, the z-transform is 1.
		% When cutoff is zero, nothing gets through the filter, so set
		% coefficients up correctly.

		b = [cutoff, 0, 0];
		a = [1, 0, 0];
		return;
	end

	% Compute biquad coefficients for lowpass filter
	resonance = max(0.0, resonance); % can't go negative
	g = 10.0^(0.05 * resonance);
	d = sqrt((4 - sqrt(16 - 16 / (g * g))) / 2);

	theta = pi * cutoff;
	sn = 0.5 * d * sin(theta);
	beta = 0.5 * (1 - sn) / (1 + sn);
	gamma = (0.5 + beta) * cos(theta);
	alpha = 0.25 * (0.5 + beta - gamma);

	b0 = 2 * alpha;
	b1 = 2 * 2 * alpha;
	b2 = 2 * alpha;
	a1 = 2 * -gamma;
	a2 = 2 * beta;

	b = [b0, b1, b2];
	a = [1.0, a1, a2];
end

function [b, a] = band_pass_2nd(f, Q, fs)
	frequency = f/(fs/2);
	% No negative frequencies allowed.
	frequency = max(0.0, frequency);

	% Don't let Q go negative, which causes an unstable filter.
	Q = max(0.0, Q);

	if frequency <= 0 || frequency >= 1
		% When the cutoff is zero, the z-transform approaches 0, if Q
		% > 0. When both Q and cutoff are zero, the z-transform is
		% pretty much undefined. What should we do in this case?
		% For now, just make the filter 0. When the cutoff is 1, the
		% z-transform also approaches 0.
		b = [0, 0, 0];
		a = [1, 0, 0];
		return;
	end
	if (Q <= 0)
		% When Q = 0, the above formulas have problems. If we
		% look at the z-transform, we can see that the limit
		% as Q->0 is 1, so set the filter that way.
		b = [1, 0, 0];
		a = [1, 0, 0];
		return;
	end

	w0 = pi * frequency;
	alpha = sin(w0) / (2 * Q);
	k = cos(w0);

	b0 = alpha;
	b1 = 0;
	b2 = -alpha;
	a0 = 1 + alpha;
	a1 = -2 * k;
	a2 = 1 - alpha;
	b = [b0 / a0 b1 / a0 b2 / a0];
	a = [1 a1 / a0 a2 / a0];
end

function [b, a] = notch_2nd(f, Q, fs)
	frequency = f/(fs/2);
	% Clip frequencies to between 0 and 1, inclusive.
	frequency = max(0.0, min(frequency, 1.0));

	% Don't let Q go negative, which causes an unstable filter.
	Q = max(0.0, Q);

	if frequency <= 0 || frequency >= 1
		% When frequency is 0 or 1, the z-transform is 1.
		b = [1, 0, 0];
		a = [1, 0, 0];
		return;
	end
	if Q <= 0
		% When Q = 0, the above formulas have problems. If we
		% look at the z-transform, we can see that the limit
		% as Q->0 is 0, so set the filter that way.
		b = [0, 0, 0];
		a = [1, 0, 0];
		return;
	end

	w0 = pi * frequency;
	alpha = sin(w0) / (2 * Q);
	k = cos(w0);

	b0 = 1;
	b1 = -2 * k;
	b2 = 1;
	a0 = 1 + alpha;
	a1 = -2 * k;
	a2 = 1 - alpha;

	b = [b0 / a0 b1 / a0 b2 / a0];
	a = [1 a1 / a0 a2 / a0];
end

function [b, a] = low_shelf_2nd_google(f, db_gain, fs)
	frequency = f/(fs/2);
	% Clip frequencies to between 0 and 1, inclusive.
	frequency = max(0.0, min(frequency, 1.0));

	A = 10.0^(db_gain / 40);

	if (frequency == 1)
		% The z-transform is a constant gain.
		b = [A * A, 0, 0];
		a = [1, 0, 0];
		return;
	end
	if (frequency <= 0)
		% When frequency is 0, the z-transform is 1.
		b = [1, 0, 0];
		a = [1, 0, 0];
		return;
	end

	w0 = pi * frequency;
	S = 1; % filter slope (1 is max value)
	alpha = 0.5 * sin(w0) * ...
	sqrt((A + 1 / A) * (1 / S - 1) + 2);
	k = cos(w0);
	k2 = 2 * sqrt(A) * alpha;
	a_plus_one = A + 1;
	a_minus_one = A - 1;

	b0 = A * (a_plus_one - a_minus_one * k + k2);
	b1 = 2 * A * (a_minus_one - a_plus_one * k);
	b2 = A * (a_plus_one - a_minus_one * k - k2);
	a0 = a_plus_one + a_minus_one * k + k2;
	a1 = -2 * (a_minus_one + a_plus_one * k);
	a2 = a_plus_one + a_minus_one * k - k2;

	b = [b0 / a0 b1 / a0 b2 / a0];
	a = [1 a1 / a0 a2 / a0];
end

function [b, a] = high_shelf_2nd_google(f, db_gain, fs)
	frequency = f/(fs/2);
	% Clip frequencies to between 0 and 1, inclusive.
	frequency = max(0.0, min(frequency, 1.0));

	A = 10.0^(db_gain / 40);

	if (frequency == 1)
		% The z-transform is 1.
		b = [1, 0, 0];
		a = [1, 0, 0];
		return;
	end
	if (frequency <= 0)
		% When frequency = 0, the filter is just a gain, A^2.
		b = [A * A, 0, 0];
		a = [1, 0, 0];
		return;
	end

	w0 = pi * frequency;
	S = 1; % filter slope (1 is max value)
	alpha = 0.5 * sin(w0) * ...
	sqrt((A + 1 / A) * (1 / S - 1) + 2);
	k = cos(w0);
	k2 = 2 * sqrt(A) * alpha;
	a_plus_one = A + 1;
	a_minus_one = A - 1;

	b0 = A * (a_plus_one + a_minus_one * k + k2);
	b1 = -2 * A * (a_minus_one + a_plus_one * k);
	b2 = A * (a_plus_one + a_minus_one * k - k2);
	a0 = a_plus_one - a_minus_one * k + k2;
	a1 = 2 * (a_minus_one - a_plus_one * k);
	a2 = a_plus_one - a_minus_one * k - k2;

	b = [b0 / a0 b1 / a0 b2 / a0];
	a = [1 a1 / a0 a2 / a0];
end

function [b, a] = my_bilinear(sb, sa, fs)
if exist('OCTAVE_VERSION', 'builtin')
        [b, a] = bilinear(sb, sa, 1/fs);
else
        [b, a] = bilinear(sb, sa, fs);
end
end

function sw = wmap(w, fs)
t = 1/fs;
sw = 2/t*tan(w*t/2);
end
