function example_crossover();

% Set the parameters here
tplg_fn = "../../topology/topology1/m4/crossover_coef_default.m4" % Control Bytes File
% Use those files with sof-ctl to update the component's configuration
blob_fn = "../../ctl/crossover_coef.blob" % Blob binary file
alsa_fn = "../../ctl/crossover_coef.txt" % ALSA CSV format file

endian = "little";

% Sampling Frequency and Frequency cut-offs for crossover
fs = 48e3;
fc_low = 200;
fc_med = 1000;
fc_high = 3000;

% 4 way crossover
num_sinks = 4;
% This array is an example on how to assign a buffer from pipeline 1 to output 0,
% buffer from pipeline 2 to output 1, etc...
% Refer to sof/src/inlude/user/crossover.h for more information on assigning
% buffers to outputs.
assign_sinks = zeros(1, 4);
assign_sinks(1) = 1; % sink[0]
assign_sinks(2) = 2; % sink[1]
assign_sinks(3) = 3; % sink[2]
assign_sinks(4) = 4; % sink[3]

% Generate zeros, poles and gain for crossover with the given frequencies
%crossover = crossover_gen_coefs(fs, fc_low); % 2 way crossover
% crossover = crossover_gen_coefs(fs, fc_low, fc_med); % 3 way crossover
crossover = crossover_gen_coefs(fs, fc_low, fc_med, fc_high); % 4 way crossover

% Convert the [a,b] coefficients to values usable with SOF
crossover_bqs = crossover_coef_quant(crossover.lp, crossover.hp);

% Convert coefficients to sof_crossover_config struct
config = crossover_generate_config(crossover_bqs, num_sinks, assign_sinks);

% Convert struct to binary blob
blob8 = crossover_build_blob(config, endian);

% Generate output files
addpath ./../common

tplg_write(tplg_fn, blob8, "CROSSOVER");
blob_write(blob_fn, blob8);
alsactl_write(alsa_fn, blob8);

% Plot Magnitude and Phase Response of each sink
crossover_plot_freq(crossover.lp, crossover.hp, fs, num_sinks);
rmpath ./../common

endfunction
