function crossover = crossover_gen_coefs(fs, fc_low, fc_mid, fc_high)

switch nargin
	case 2, crossover = crossover_generate_2way(fs, fc_low);
	case 3, crossover = crossover_generate_3way(fs, fc_low, fc_mid);
	case 4, crossover = crossover_generate_4way(fs, fc_low, fc_mid, fc_high);
	otherwise, error("Invalid number of arguments");
end

end

function crossover_2way = crossover_generate_2way(fs, fc)
	crossover_2way.lp = [lp_iir(fs, fc, 0)];
	crossover_2way.hp = [hp_iir(fs, fc, 0)];
end

function crossover_3way = crossover_generate_3way(fs, fc_low, fc_high)
	% Duplicate one set of coefficients. The duplicate set will be used to merge back the
	% output that is out of phase.
	crossover_3way.lp = [lp_iir(fs, fc_low, 0) lp_iir(fs, fc_high, 0) lp_iir(fs, fc_high, 0)];
	crossover_3way.hp = [hp_iir(fs, fc_low, 0) hp_iir(fs, fc_high, 0) hp_iir(fs, fc_high, 0)];
end

function crossover_4way = crossover_generate_4way(fs, fc_low, fc_mid, fc_high)
	crossover_4way.lp = [lp_iir(fs, fc_low, 0) lp_iir(fs, fc_mid, 0) lp_iir(fs, fc_high, 0)];
	crossover_4way.hp = [hp_iir(fs, fc_low, 0) hp_iir(fs, fc_mid, 0) hp_iir(fs, fc_high, 0)];
end

% Generate the a,b coefficients for a second order
% low pass butterworth filter
function lp = lp_iir(fs, fc, gain_db)
[lp.b, lp.a] = low_pass_2nd_resonance(fc, 0, fs);
end

% Generate the a,b coefficients for a second order
% low pass butterworth filter
function hp = hp_iir(fs, fc, gain_db)
[hp.b, hp.a] = high_pass_2nd_resonance(fc, 0, fs);
end

function [b, a] = high_pass_2nd_resonance(f, resonance, fs)
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

function [b, a] = low_pass_2nd_resonance(f, resonance, fs)
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
