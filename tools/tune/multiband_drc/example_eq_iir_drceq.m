function example_eq_iir_drceq();

tplg_fn = "../../topology/m4/eq_iir_coef_drceq.m4"; % Control Bytes File
% Use those files with sof-ctl to update the component's configuration
blob_fn = "../../ctl/eq_iir_coef_drceq.blob"; % Blob binary file
alsa_fn = "../../ctl/eq_iir_coef_drceq.txt"; % ALSA CSV format file

endian = "little";

sample_rate = 48000;
nyquist = sample_rate / 2;

%% Input parameters
% 1-st channel 1-st biquad parameter
eq_2ch_params(1).type = 2;
eq_2ch_params(1).freq = 195;
eq_2ch_params(1).Q = 0;
eq_2ch_params(1).gain = 0;
% 2-nd channel 1-st biquad parameter
eq_2ch_params(2).type = 2;
eq_2ch_params(2).freq = 195;
eq_2ch_params(2).Q = 0;
eq_2ch_params(2).gain = 0;
% 1-st channel 2-nd biquad parameter
eq_2ch_params(3).type = 6;
eq_2ch_params(3).freq = 520;
eq_2ch_params(3).Q = 1.8;
eq_2ch_params(3).gain = -10;
% 2-nd channel 2-nd biquad parameter
eq_2ch_params(4).type = 6;
eq_2ch_params(4).freq = 520;
eq_2ch_params(4).Q = 1.8;
eq_2ch_params(4).gain = -10;
% 1-st channel 3-rd biquad parameter
eq_2ch_params(5).type = 6;
eq_2ch_params(5).freq = 2900;
eq_2ch_params(5).Q = 0.5;
eq_2ch_params(5).gain = -9;
% 2-nd channel 3-rd biquad parameter
eq_2ch_params(6).type = 6;
eq_2ch_params(6).freq = 2900;
eq_2ch_params(6).Q = 0.5;
eq_2ch_params(6).gain = -9;
% 1-st channel 4-th biquad parameter
eq_2ch_params(7).type = 6;
eq_2ch_params(7).freq = 12000;
eq_2ch_params(7).Q = 0.8;
eq_2ch_params(7).gain = 6;
% 2-nd channel 4-th biquad parameter
eq_2ch_params(8).type = 6;
eq_2ch_params(8).freq = 12000;
eq_2ch_params(8).Q = 0.8;
eq_2ch_params(8).gain = 6;
% 1-st channel 5-th biquad parameter
eq_2ch_params(9).type = 6;
eq_2ch_params(9).freq = 4000;
eq_2ch_params(9).Q = 3.5;
eq_2ch_params(9).gain = -4.5;
% 2-nd channel 5-th biquad parameter
eq_2ch_params(10).type = 6;
eq_2ch_params(10).freq = 4000;
eq_2ch_params(10).Q = 3.5;
eq_2ch_params(10).gain = -4.5;
% 1-st channel 6-th biquad parameter
eq_2ch_params(11).type = 6;
eq_2ch_params(11).freq = 1100;
eq_2ch_params(11).Q = 2;
eq_2ch_params(11).gain = -5.5;
% 2-nd channel 6-th biquad parameter
eq_2ch_params(12).type = 6;
eq_2ch_params(12).freq = 1100;
eq_2ch_params(12).Q = 2;
eq_2ch_params(12).gain = -5.5;
% 1-st channel 7-th biquad parameter
eq_2ch_params(13).type = 6;
eq_2ch_params(13).freq = 3100;
eq_2ch_params(13).Q = 4;
eq_2ch_params(13).gain = -2;
% 2-nd channel 7-th biquad parameter
eq_2ch_params(14).type = 6;
eq_2ch_params(14).freq = 3100;
eq_2ch_params(14).Q = 4;
eq_2ch_params(14).gain = -2;
% 1-st channel 8-th biquad parameter
eq_2ch_params(15).type = 6;
eq_2ch_params(15).freq = 220;
eq_2ch_params(15).Q = 2;
eq_2ch_params(15).gain = -3.5;
% 2-nd channel 8-th biquad parameter
eq_2ch_params(16).type = 6;
eq_2ch_params(16).freq = 220;
eq_2ch_params(16).Q = 2;
eq_2ch_params(16).gain = -3.5;

%% Calculate biquad coefficients
eq1_biquad_coefs = cell(1, length(eq_2ch_params)/2);
eq2_biquad_coefs = cell(1, length(eq_2ch_params)/2);
for i = 1:length(eq_2ch_params)/2
	eq_2ch_params(i*2-1).freq /= nyquist;
	eq1_biquad_coefs(i) = biquad_gen_df2t_coefs(eq_2ch_params(i*2-1));
	eq_2ch_params(i*2).freq /= nyquist;
	eq2_biquad_coefs(i) = biquad_gen_df2t_coefs(eq_2ch_params(i*2));
endfor

eq1_biquad_coefs = eq_adjust_shift_gain(eq1_biquad_coefs)
eq2_biquad_coefs = eq_adjust_shift_gain(eq2_biquad_coefs)

%% Generate quantized EQ headers
eq1_header_quant = eq_gen_header_quant(eq1_biquad_coefs);
eq2_header_quant = eq_gen_header_quant(eq2_biquad_coefs);

%% Build config blob
addpath ../eq

channels_in_config = 2;
num_responses = 2;
assign_response = [0 1];
bm = eq_iir_blob_merge(channels_in_config, ...
		       num_responses, ...
		       assign_response, ...
		       [eq1_header_quant eq2_header_quant]);

bp = eq_iir_blob_pack(bm, endian);

rmpath ../eq

%% Generate output files
addpath ../common

tplg_write(tplg_fn, bp, "EQIIR");
blob_write(blob_fn, bp);
alsactl_write(alsa_fn, bp);

rmpath ../common

endfunction
