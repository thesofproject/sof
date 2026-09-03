function coefs = sof_drc_gen_coefs(params, sample_rate)

if exist('OCTAVE_VERSION', 'builtin')
	pkg load control;
end

% Print out params
params

coefs.enabled = params.enabled;

% Calculate static curve coefficients
coefs.db_threshold = params.threshold;
coefs.db_knee = params.knee;
coefs.ratio = params.ratio;
coefs.pre_delay_time = params.pre_delay;
coefs.linear_threshold = db2mag(coefs.db_threshold);
coefs.slope = 1 / coefs.ratio;

k = k_at_slope(coefs.db_threshold, coefs.db_knee, coefs.linear_threshold, coefs.slope);
coefs.K = k;
coefs.knee_alpha = coefs.linear_threshold + 1 / k;
coefs.knee_beta = -exp(k * coefs.linear_threshold) / k;
coefs.knee_threshold = db2mag(coefs.db_threshold + coefs.db_knee);

y0 = knee_curve(coefs.linear_threshold, coefs.knee_threshold, k);
coefs.ratio_base = y0 * (coefs.knee_threshold ^ (-coefs.slope));

% Calculate makeup gain coefficients
full_range_makeup_gain = (1 / coefs.ratio_base) ^ 0.6; % Empirical/perceptual tuning
coefs.master_linear_gain = db2mag(params.post_gain) * full_range_makeup_gain;
coefs.master_linear_gain_db = 20*log10(coefs.master_linear_gain);

% Calculate attack time coefficients
attack_time = max(0.001, params.attack);
coefs.one_over_attack_frames = 1 / (attack_time * sample_rate);

% Calculate release time coefficients
release_frames = params.release * sample_rate;
sat_release_time = 0.0025;
sat_release_frames = sat_release_time * sample_rate;
coefs.sat_release_frames_inv_neg = -1 / sat_release_frames;
coefs.sat_release_rate_at_neg_two_db = db2mag(-2 * coefs.sat_release_frames_inv_neg) - 1;

coefs.kSpacingDb = params.release_spacing;

% Create a smooth function which passes through four points.
% Polynomial of the form y = a + b*x + c*x^2 + d*x^3 + e*x^4
y = params.release_zone .* release_frames;

% All of these coefficients were derived for 4th order polynomial curve fitting
% where the y values match the evenly spaced x values as follows:
%   (y1 : x == 0, y2 : x == 1, y3 : x == 2, y4 : x == 3)
coefs.kA = 0.9999999999999998 * y(1) + 1.8432219684323923e-16 * y(2) ...
	- 1.9373394351676423e-16 * y(3) + 8.824516011816245e-18 * y(4);
coefs.kB = -1.5788320352845888 * y(1) + 2.3305837032074286 * y(2) ...
	- 0.9141194204840429 * y(3) + 0.1623677525612032 * y(4);
coefs.kC = 0.5334142869106424 * y(1) - 1.272736789213631 * y(2) ...
	+ 0.9258856042207512 * y(3) - 0.18656310191776226 * y(4);
coefs.kD = 0.08783463138207234 * y(1) - 0.1694162967925622 * y(2) ...
	+ 0.08588057951595272 * y(3) - 0.00429891410546283 * y(4);
coefs.kE = -0.042416883008123074 * y(1) + 0.1115693827987602 * y(2) ...
	- 0.09764676325265872 * y(3) + 0.028494263462021576 * y(4);

% Print out coefs
coefs

end


function k = k_at_slope(db_threshold, db_knee, linear_threshold, desired_slope)

% Approximate k given initial values
x = db2mag(db_threshold + db_knee);
min_k = 0.1;
max_k = 10000;
k = 5;

for i = 1:15
	% A high value for k will more quickly asymptotically approach a slope of 0
	slope = slope_at(linear_threshold, x, k);
	if slope < desired_slope
		max_k = k; % k is too high
	else
		min_k = k; % k is too low
	end

	% Re-calculate based on geometric mean
	k = sqrt(min_k * max_k);
end

end


function slope = slope_at(linear_threshold, x, k)

if x < linear_threshold
	slope = 1;
else
	x2 = x * 1.001;
	x_db  = mag2db(x);
	x2_db = mag2db(x2);
	y_db = mag2db(knee_curve(linear_threshold, x, k));
	y2_db = mag2db(knee_curve(linear_threshold, x2, k));

	slope = (y2_db - y_db) / (x2_db - x_db);
end

end


function y = knee_curve(linear_threshold, x, k)

if x < linear_threshold
	y = x;
else
	y = linear_threshold + (1 - exp(-k * (x - linear_threshold))) / k;
end

end
