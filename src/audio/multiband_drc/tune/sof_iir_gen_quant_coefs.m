function [emp_coefs, deemp_coefs] = sof_iir_gen_quant_coefs(params, sample_rate, enable)

stage_gain = params.stage_gain;
stage_ratio = params.stage_ratio;
anchor_freq = params.anchor;

emp = cell(1, 2);
deemp = cell(1, 2);
% Generate the coefficients of (de)emphasis for the 1-st stage of biquads
[emp(1), deemp(1)] = emp_deemp_stage_biquad(stage_gain, anchor_freq, ...
					    anchor_freq / stage_ratio);

anchor_freq = anchor_freq / (stage_ratio * stage_ratio);

% Generate the coefficients of (de)emphasis for the 2-nd stage of biquads
[emp(2), deemp(2)] = emp_deemp_stage_biquad(stage_gain, anchor_freq, ...
                                            anchor_freq / stage_ratio);

% Adjust the stage gain (push gains to the last stage) of emphasis filter
[emp(1), emp(2)] = stage_gain_adjust(emp(1), emp(2));

% Plot emp and deemp
if enable
	plot_emp_deem(emp, deemp, sample_rate);
end

% Convert the coefficients to values usable with SOF
emp_coefs = iir_coef_quant(emp);
deemp_coefs = iir_coef_quant(deemp);

end

function [emp, deemp] = emp_deemp_stage_biquad(gain, f1, f2)

[z1, p1] = emp_stage_roots(gain, f1);
[z2, p2] = emp_stage_roots(gain, f2);

b0 = 1;
b1 = -(z1 + z2);
b2 = z1 * z2;
a0 = 1;
a1 = -(p1 + p2);
a2 = p1 * p2;

% Gain compensation to make 0dB at 0Hz
% For emphasis filter, alpha should be > 1. Record the alpha gain and then we
% will do the stage gain adjustment afterwards.
alpha = (a0 + a1 + a2) / (b0 + b1 + b2);

%     [ a2   a1  b2  b1  b0  shift gain]
emp = [-a2, -a1, b2, b1, b0, 0, alpha];

% For deemphasis filter, beta should be < 1. Multiply beta directly to b coeffs
% to have a healthy scaling of biquads internally.
beta = (b0 + b1 + b2) / (a0 + a1 + a2);

%       [ a2   a1         b2         b1         b0  shift gain]
deemp = [-b2, -b1, a2 * beta, a1 * beta, a0 * beta, 0, 1.0];

end

function [zero, pole] = emp_stage_roots(gain, normalized_freq)

gk = 1 - gain / 20;
f1 = normalized_freq * gk;
f2 = normalized_freq / gk;
zero = exp(-f1 * pi);
pole = exp(-f2 * pi);

end

function quant_coefs = iir_coef_quant(coefs)

bits_iir = 32; % Q2.30
qf_iir = 30;

bits_gain = 16; % Q2.14
qf_gain = 14;

quant_coefs = cell(1, 2);
for i = 1:length(coefs)
	coef = cell2mat(coefs(i));
	quant_ab = sof_eq_coef_quant(coef(1:5), bits_iir, qf_iir);
	quant_gain = sof_eq_coef_quant(coef(7), bits_gain, qf_gain);
	quant_coefs(i) = [quant_ab coef(6) quant_gain];
end

quant_coefs = cell2mat(quant_coefs);

end

function [bq1, bq2] = stage_gain_adjust(prev_bq1, prev_bq2)

prev_bq1 = cell2mat(prev_bq1);
prev_bq2 = cell2mat(prev_bq2);

[rshift, gain] = decompose_gain(prev_bq1(7) * prev_bq2(7));
bq1 = [prev_bq1(1:5) 0 1.0];
bq2 = [prev_bq2(1:5) rshift gain];

end

function [rshift, gain] = decompose_gain(prev_gain)

max_abs_val = 2; % Q2.14
rshift = 0;
gain = prev_gain;

while (abs(gain) >= max_abs_val)
	gain = gain / 2;
	rshift = rshift - 1; % left-shift in shift stage
end

end

function plot_emp_deem(emp, deemp, fs)

f = logspace(log10(10), log10(fs/2), 100);

[eb1, ea1] = get_biquad(cell2mat(emp(1)));
[eb2, ea2] = get_biquad(cell2mat(emp(2)));
eb = conv(eb1, eb2);
ea = conv(ea1, ea2);
he = freqz(eb, ea, f, fs);

[db1, da1] = get_biquad(cell2mat(deemp(1)));
[db2, da2] = get_biquad(cell2mat(deemp(2)));
db = conv(db1, db2);
da = conv(da1, da2);
hd = freqz(db, da, f, fs);
hed = he .* hd;

figure
subplot(3,1,1);
semilogx(f, 20*log10(he));
grid on;
ylabel('Emp (dB)');
title('Emphasis, de-emphasis responses');
subplot(3,1,2);
semilogx(f, 20*log10(hd));
grid on;
ylabel('De-emp (dB)');
subplot(3,1,3);
semilogx(f, 20*log10(hed));
grid on;
ylabel('Combined (dB)')
xlabel('Frequency (Hz)');

end

function [b, a] = get_biquad(bq)

	a = [1 -bq(2:-1:1)];
	b = [bq(5:-1:3)];
	shift = bq(6);
	gain = bq(7) * 2^(-shift);
	b = b * gain;
end
