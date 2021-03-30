function eq_header_quant = eq_gen_header_quant(eq_biquad_coefs);

n_header = 6;
n_biquad = 7;
num_sections = length(eq_biquad_coefs);
eq_header_quant = int32(zeros(1, n_header + n_biquad * num_sections));
eq_header_quant(1) = num_sections;
eq_header_quant(2) = num_sections;  % num_sections_in_series
% eq_header_quant(3:6) are reserved zeros

m = n_header + 1;
for i = 1:num_sections
	eq_header_quant(m:m+n_biquad-1) = biquad_coef_quant(eq_biquad_coefs(i));
	m += n_biquad;
endfor

endfunction

function coef_quant = biquad_coef_quant(coef);

bits_bq = 32;  % Q2.30
qf_bq = 30;

bits_gain = 16;  % Q2.14
qf_gain = 14;

addpath ../eq

coef = cell2mat(coef);

quant_ab = eq_coef_quant(coef(1:5), bits_bq, qf_bq);
quant_gain = eq_coef_quant(coef(7), bits_gain, qf_gain);
coef_quant = [quant_ab coef(6) quant_gain];

rmpath ../eq

endfunction
