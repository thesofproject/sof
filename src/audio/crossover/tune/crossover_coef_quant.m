function crossover_quant = crossover_coef_quant(lowpass, highpass);

bits_iir = 32; % Q2.30
qf_iir = 30;

addpath ./../eq

if length(lowpass) ~= length(highpass)
	error("length of lowpass and highpass array do not match");
end

n = length(lowpass);
crossover_quant.lp_coef = cell(1,n);
crossover_quant.hp_coef = cell(1,n);
for i = 1:n
	lp = lowpass(i);
	hp = highpass(i);
	lp_a = eq_coef_quant(-lp.a(3:-1:2), bits_iir, qf_iir);
	lp_b = eq_coef_quant(lp.b(3:-1:1), bits_iir, qf_iir);
	hp_a = eq_coef_quant(-hp.a(3:-1:2), bits_iir, qf_iir);
	hp_b = eq_coef_quant(hp.b(3:-1:1), bits_iir, qf_iir);

	crossover_quant.lp_coef(i) = [lp_a lp_b 0 16384];
	crossover_quant.hp_coef(i) = [hp_a hp_b 0 16384];
end

crossover_quant.lp_coef = cell2mat(crossover_quant.lp_coef);
crossover_quant.hp_coef = cell2mat(crossover_quant.hp_coef);

rmpath ./../eq
end
