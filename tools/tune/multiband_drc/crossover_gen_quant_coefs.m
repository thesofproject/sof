function crossover_coefs = crossover_gen_quant_coefs(num_bands, sample_rate,
						     fc_low, fc_med, fc_high);

% De-normalize cutoff frequencies in respect to nyquist (half of sample rate)
fc_low = fc_low * sample_rate / 2;
fc_med = fc_med * sample_rate / 2;
fc_high = fc_high * sample_rate / 2;

addpath ../crossover

filter_len = 3;
crossover.lp = cell(1:filter_len);
crossover.hp = cell(1:filter_len);
% Generate zeros, poles and gain for crossover with the given frequencies
% Extend the length of lp and hp to 3 (filter_len) by flat_filter if necessary
if (num_bands == 1)
	% Pass-through
	crossover.lp = [flat_filter() flat_filter() flat_filter()];
	crossover.hp = [flat_filter() flat_filter() flat_filter()];
elseif (num_bands == 2)
	% 2-way crossover
	crossover = crossover_gen_coefs(sample_rate, fc_low);
	crossover.lp = [crossover.lp(1) flat_filter() flat_filter()];
	crossover.hp = [crossover.hp(1) flat_filter() flat_filter()];
elseif (num_bands == 3)
	% 3-way crossover
	crossover = crossover_gen_coefs(sample_rate, fc_low, fc_med);
else % (num_bands == 4)
	% 4-way crossover
	crossover = crossover_gen_coefs(sample_rate, fc_low, fc_med, fc_high);
endif

assert(length(crossover.lp) == filter_len && length(crossover.hp) == filter_len);

% Print crossover
for i = 1:filter_len
	crossover.lp(i)
	crossover.hp(i)
end

% Convert the [a,b] coefficients to values usable with SOF
crossover_bqs = crossover_coef_quant(crossover.lp, crossover.hp);

rmpath ../crossover

j = 1;
k = 1;
for i = 1:filter_len
	crossover_coefs(k:k+6) = crossover_bqs.lp_coef(j:j+6); k = k + 7;
	crossover_coefs(k:k+6) = crossover_bqs.hp_coef(j:j+6); k = k + 7;
	j = j + 7;
end

end

function flat = flat_filter();

% Flat response y[n] = x[n] (only b0=1.0)
flat.b = [1.0, 0, 0];
flat.a = [0, 0, 0];

end
