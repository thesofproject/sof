function biquad_coefs = biquad_gen_df2t_coefs(params);

switch (params.type)
	case 0  % NONE
		biquad_coefs = [0.0 0.0 0.0 0.0 1.0 0 1.0];
	case 1  % LOWPASS
		biquad_coefs = lowpass_coefs(params.freq, params.Q);
	case 2  % HIGHPASS
		biquad_coefs = highpass_coefs(params.freq, params.Q);
	case 3  % BANDPASS
		biquad_coefs = bandpass_coefs(params.freq, params.Q);
	case 4  % LOWSHELF
		biquad_coefs = lowshelf_coefs(params.freq, params.gain);
	case 5  % HIGHSHELF
		biquad_coefs = highshelf_coefs(params.freq, params.gain);
	case 6  % PEAKING
		biquad_coefs = peaking_coefs(params.freq, params.Q, params.gain);
	case 7  % NOTCH
		biquad_coefs = lowshelf_coefs(params.freq, params.Q);
	case 8  % ALLPASS
		biquad_coefs = allpass_coefs(params.freq, params.Q);
	otherwise
		error ("invalid type: %d (available: 0~8)", params.type);
endswitch

endfunction

function biquad_coefs = lowpass_coefs(freq, Q);

% Limit cutoff to 0 to 1.
cutoff = max(0.0, min(freq, 1.0));

if (cutoff == 1.0 || cutoff == 0.0)
	% When cutoff is 1, the z-transform is 1.
	% When cutoff is zero, nothing gets through the filter, so set
	% coefficients up correctly.
	biquad_coefs = [0.0 0.0 0.0 0.0 cutoff 0 1.0];
	return;
endif

% Compute biquad coefficients for lowpass filter
resonance = max(0.0, Q); % can't go negative
g = 10.0 ** (0.05 * resonance);
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

biquad_coefs = [-a2 -a1 b2 b1 b0 0 1.0];

endfunction

function biquad_coefs = highpass_coefs(freq, Q);

% Limit cutoff to 0 to 1.
cutoff = max(0.0, min(freq, 1.0));

if (cutoff == 1.0 || cutoff == 0.0)
	% When cutoff is one, the z-transform is 0.
	% When cutoff is zero, we need to be careful because the above
	% gives a quadratic divided by the same quadratic, with poles
	% and zeros on the unit circle in the same place. When cutoff
	% is zero, the z-transform is 1.
	biquad_coefs = [0.0 0.0 0.0 0.0 (1.0 - cutoff) 0 1.0];
	return;
endif

% Compute biquad coefficients for highpass filter
resonance = max(0.0, Q);  % can't go negative
g = 10.0 ** (0.05 * resonance);
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

biquad_coefs = [-a2 -a1 b2 b1 b0 0 1.0];

endfunction

function biquad_coefs = bandpass_coefs(freq, Q);

% No negative frequencies allowed.
if (freq <= 0.0 || freq >= 1.0)
	% When the cutoff is zero, the z-transform approaches 0, if Q
	% > 0. When both Q and cutoff are zero, the z-transform is
	% pretty much undefined. What should we do in this case?
	% For now, just make the filter 0. When the cutoff is 1, the
	% z-transform also approaches 0.
	biquad_coefs = [0.0 0.0 0.0 0.0 0.0 0 1.0];
	return;
endif

% Don't let Q go negative, which causes an unstable filter.
if (Q <= 0.0)
	% When Q = 0, the above formulas have problems. If we
	% look at the z-transform, we can see that the limit
	% as Q->0 is 1, so set the filter that way.
	biquad_coefs = [0.0 0.0 0.0 0.0 1.0 0 1.0];
	return;
endif

w0 = pi * freq;
alpha = sin(w0) / (2 * Q);
k = cos(w0);

b0 = alpha;
b1 = 0;
b2 = -alpha;
a0 = 1 + alpha;
a1 = -2 * k;
a2 = 1 - alpha;

biquad_coefs = [-(a2 / a0) -(a1 / a0) (b2 / a0) (b1 / a0) (b0 / a0) 0 1.0];

endfunction

function biquad_coefs = lowshelf_coefs(freq, gain);

% Clip frequencies to between 0 and 1, inclusive.
if (freq <= 0.0)
	% When frequency is 0, the z-transform is 1.
	biquad_coefs = [0.0 0.0 0.0 0.0 1.0 0 1.0];
	return;
endif
if (freq >= 1.0)
	% The z-transform is a constant gain.
	biquad_coefs = [0.0 0.0 0.0 0.0 (A * A) 0 1.0];
	return;
endif

A = 10.0 ** (gain / 40);

w0 = pi * freq;
S = 1; % filter slope (1 is max value)
alpha = 0.5 * sin(w0) * sqrt((A + 1 / A) * (1 / S - 1) + 2);
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

biquad_coefs = [-(a2 / a0) -(a1 / a0) (b2 / a0) (b1 / a0) (b0 / a0) 0 1.0];

endfunction

function biquad_coefs = highshelf_coefs(freq, gain);

% Clip frequencies to between 0 and 1, inclusive.
if (freq <= 0.0)
	% When frequency = 0, the filter is just a gain, A^2.
	biquad_coefs = [0.0 0.0 0.0 0.0 (A * A) 0 1.0];
	return;
endif
if (freq >= 1.0)
	% The z-transform is 1.
	biquad_coefs = [0.0 0.0 0.0 0.0 1.0 0 1.0];
	return;
endif

A = 10.0 ** (gain / 40);

w0 = pi * freq;
S = 1; % filter slope (1 is max value)
alpha = 0.5 * sin(w0) * sqrt((A + 1 / A) * (1 / S - 1) + 2);
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

biquad_coefs = [-(a2 / a0) -(a1 / a0) (b2 / a0) (b1 / a0) (b0 / a0) 0 1.0];

endfunction

function biquad_coefs = peaking_coefs(freq, Q, gain);

% Clip frequencies to between 0 and 1, inclusive.
if (freq <= 0.0 || freq >= 1.0)
	% When frequency is 0 or 1, the z-transform is 1.
	biquad_coefs = [0.0 0.0 0.0 0.0 1.0 0 1.0];
	return;
endif

A = 10.0 ** (gain / 40);

% Don't let Q go negative, which causes an unstable filter.
if (Q <= 0.0)
	% When Q <= 0, the above formulas have problems. If we
	% look at the z-transform, we can see that the limit
	% as Q->0 is A^2, so set the filter that way.
	biquad_coefs = [0.0 0.0 0.0 0.0 (A * A) 0 1.0];
        return;
endif

w0 = pi * freq;
alpha = sin(w0) / (2 * Q);
k = cos(w0);

b0 = 1 + alpha * A;
b1 = -2 * k;
b2 = 1 - alpha * A;
a0 = 1 + alpha / A;
a1 = -2 * k;
a2 = 1 - alpha / A;

biquad_coefs = [-(a2 / a0) -(a1 / a0) (b2 / a0) (b1 / a0) (b0 / a0) 0 1.0];

endfunction

function biquad_coefs = notch_coefs(freq, Q);

% Clip frequencies to between 0 and 1, inclusive.
if (freq <= 0.0 || freq >= 1.0)
	% When frequency is 0 or 1, the z-transform is 1.
	biquad_coefs = [0.0 0.0 0.0 0.0 1.0 0 1.0];
	return;
endif

% Don't let Q go negative, which causes an unstable filter.
if (Q <= 0.0)
	% When Q = 0, the above formulas have problems. If we
	% look at the z-transform, we can see that the limit
	% as Q->0 is 0, so set the filter that way.
	biquad_coefs = [0.0 0.0 0.0 0.0 0.0 0 1.0];
	return;
endif

w0 = pi * freq;
alpha = sin(w0) / (2 * Q);
k = cos(w0);

b0 = 1;
b1 = -2 * k;
b2 = 1;
a0 = 1 + alpha;
a1 = -2 * k;
a2 = 1 - alpha;

biquad_coefs = [-(a2 / a0) -(a1 / a0) (b2 / a0) (b1 / a0) (b0 / a0) 0 1.0];

endfunction

function biquad_coefs = allpass_coefs(freq, Q);

% Clip frequencies to between 0 and 1, inclusive.
if (freq <= 0.0 || freq >= 1.0)
	% When frequency is 0 or 1, the z-transform is 1.
	biquad_coefs = [0.0 0.0 0.0 0.0 1.0 0 1.0];
	return;
endif

% Don't let Q go negative, which causes an unstable filter.
if (Q <= 0.0)
	% When Q = 0, the above formulas have problems. If we
	% look at the z-transform, we can see that the limit
	% as Q->0 is -1, so set the filter that way.
	biquad_coefs = [0.0 0.0 0.0 0.0 -1.0 0 1.0];
	return;
endif

w0 = pi * frequency;
alpha = sin(w0) / (2 * Q);
k = cos(w0);

b0 = 1 - alpha;
b1 = -2 * k;
b2 = 1 + alpha;
a0 = 1 + alpha;
a1 = -2 * k;
a2 = 1 - alpha;

biquad_coefs = [-(a2 / a0) -(a1 / a0) (b2 / a0) (b1 / a0) (b0 / a0) 0 1.0];

endfunction
