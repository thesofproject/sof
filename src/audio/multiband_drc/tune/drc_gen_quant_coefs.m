function drc_coefs = drc_gen_quant_coefs(num_bands, sample_rate, params);

for i = 1:num_bands
	coefs = drc_gen_coefs(params(i), sample_rate);
	drc_coefs(i) = drc_generate_config(coefs);
end

end
