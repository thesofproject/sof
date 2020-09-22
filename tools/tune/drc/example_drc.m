function example_drc();

% Set the parameters here
tplg_fn = "../../topology/m4/drc_coef_default.m4" % Control Bytes File
% Use those files with sof-ctl to update the component's configuration
blob_fn = "../../ctl/drc_coef.blob" % Blob binary file
alsa_fn = "../../ctl/drc_coef.txt" % ALSA CSV format file

endian = "little";

% DRC enabled
enabled = 1

% Convert coefficients to sof_drc_config struct
config = drc_generate_config(enabled);

% Convert struct to binary blob
blob8 = drc_build_blob(config, endian);

% Generate output files
addpath ./../common

tplg_write(tplg_fn, blob8, "DRC");
blob_write(blob_fn, blob8);
alsactl_write(alsa_fn, blob8);

rmpath ./../common

endfunction

