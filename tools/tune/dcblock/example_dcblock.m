function example_dcblock();

% Set the parameters here
tplg_fn = "../../topology/topology1/m4/dcblock_coef_default.m4" % Control Bytes File
% Use those files with sof-ctl to update the component's configuration
blob_fn = "../../ctl/dcblock_coef.blob" % Blob binary file
alsa_fn = "../../ctl/dcblock_coef.txt" % ALSA CSV format file

endian = "little";
R_coeffs = [0.98, 0.98, 0.98, 0.98, 0.98, 0.98, 0.98, 0.98];

blob8 = dcblock_build_blob(R_coeffs, endian);

% Generate output files
addpath ./../common

tplg_write(tplg_fn, blob8, "DCBLOCK");
blob_write(blob_fn, blob8);
alsactl_write(alsa_fn, blob8);

% Plot Filter's Transfer Function and Step Response
% As an example, plot the graphs of the first coefficient
fs = 48e3
dcblock_plot_transferfn(R_coeffs(1), fs);
figure
dcblock_plot_stepfn(R_coeffs(1), fs);

rmpath ./../common

endfunction
