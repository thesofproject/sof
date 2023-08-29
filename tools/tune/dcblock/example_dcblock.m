function example_dcblock()

% Set the parameters here
tplg1_fn = "../../topology/topology1/m4/dcblock_coef_default.m4"; % Control Bytes File
tplg2_fn = "../../topology/topology2/include/components/dcblock/default.conf";
% Use those files with sof-ctl to update the component's configuration
blob3_fn = "../../ctl/ipc3/dcblock_coef.blob"; % Blob binary file
alsa3_fn = "../../ctl/ipc3/dcblock_coef.txt"; % ALSA CSV format file
blob4_fn = "../../ctl/ipc4/dcblock_coef.blob"; % Blob binary file
alsa4_fn = "../../ctl/ipc4/dcblock_coef.txt"; % ALSA CSV format file

endian = "little";
R_coeffs = [0.98, 0.98, 0.98, 0.98, 0.98, 0.98, 0.98, 0.98];

addpath ./../common

blob8 = dcblock_build_blob(R_coeffs, endian);
blob8_ipc4 = dcblock_build_blob(R_coeffs, endian, 4);

% Generate output files
tplg_write(tplg1_fn, blob8, "DCBLOCK");
blob_write(blob3_fn, blob8);
alsactl_write(alsa3_fn, blob8);

tplg2_write(tplg2_fn, blob8_ipc4, "dcblock_config", "Exported with script example_dcblock.m");
blob_write(blob4_fn, blob8_ipc4);
alsactl_write(alsa4_fn, blob8_ipc4);

% Plot Filter's Transfer Function and Step Response
% As an example, plot the graphs of the first coefficient
fs = 48e3;
dcblock_plot_transferfn(R_coeffs(1), fs);
figure
dcblock_plot_stepfn(R_coeffs(1), fs);

rmpath ./../common

end
