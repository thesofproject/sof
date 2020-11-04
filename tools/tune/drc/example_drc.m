function example_drc();

% Set the parameters here
tplg_fn = "../../topology/m4/drc_coef_default.m4"; % Control Bytes File
% Use those files with sof-ctl to update the component's configuration
blob_fn = "../../ctl/drc_coef.blob"; % Blob binary file
alsa_fn = "../../ctl/drc_coef.txt"; % ALSA CSV format file

endian = "little";

sample_rate = 48000;

% The parameters of the DRC compressor
% enabled: 1 to enable the compressor, 0 to disable it
params.enabled = 0;
% threshold: The value above which the compression starts, in dB
params.threshold = -24;
% knee: The value above which the knee region starts, in dB
params.knee = 30;
% ratio: The input/output dB ratio after the knee region
params.ratio = 12;
% attack: The time to reduce the gain by 10dB, in seconds
params.attack = 0.003;
% release: The time to increase the gain by 10dB, in seconds
params.release = 0.25;
% pre_delay: The lookahead time for the compressor, in seconds
params.pre_delay = 0.006;
% release_zone[4]: The adaptive release curve parameters
params.release_zone = [0.09 0.16 0.42 0.98];
% release_spacing: The value of spacing per frame while releasing, in dB
params.release_spacing = 5;
% post_gain: The static boost value in output, in dB
params.post_gain = 0;

% Generate coefficients for DRC with the given parameters
coefs = drc_gen_coefs(params, sample_rate);

% Convert coefficients to sof_drc_config struct
config = drc_generate_config(coefs);

% Convert struct to binary blob
blob8 = drc_build_blob(config, endian);

% Generate output files
addpath ./../common

tplg_write(tplg_fn, blob8, "DRC");
blob_write(blob_fn, blob8);
alsactl_write(alsa_fn, blob8);

% Plot x-y response in dB
drc_plot_db_curve(coefs);

rmpath ./../common

endfunction
