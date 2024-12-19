function sof_example_dcblock()

% Default blob, about 150 Hz cut-off @ 48 kHz
prm.fc = [];
prm.fs = [];
prm.R_coeffs = [0.98, 0.98, 0.98, 0.98, 0.98, 0.98, 0.98, 0.98];
prm.id = "default";
dcblock_blob_calculate(prm)

% Generate a set configuration blobs for 16 and 48 kHz rate
% The selected high-pass cut-off frequencies are in range 20 - 200 Hz:
% 20, 30, 40, 50, 80, 100, and 200 Hz
%
% Select for applications one that is best tradeoff between DC level
% settle time and the lowest frequency that should pass without
% much attenuation. Cut-off frequency by definition is the point where
% the attenuation is 3.01 dB.

prm.R_coeffs = [];
for fs = [16e3 48e3]
	for fc = [20 30 40 50 80 100 200]
		prm.id = sprintf("%dhz_%dkhz", round(fc), round(fs / 1000));
		prm.fs = fs;
		prm.fc = fc;
		dcblock_blob_calculate(prm)
	end
end

end

function dcblock_blob_calculate(prm)

% Set the parameters here
sof_tools = '../../../../tools';
sof_tplg = fullfile(sof_tools, 'topology');
sof_ctl = fullfile(sof_tools, 'ctl');
tplg1_fn = sprintf("%s/topology1/m4/dcblock_coef_%s.m4", sof_tplg, prm.id); % Control Bytes File
tplg2_fn = sprintf("%s/topology2/include/components/dcblock/%s.conf", sof_tplg, prm.id);
% Use those files with sof-ctl to update the component's configuration
blob3_fn = sprintf("%s/ipc3/dcblock/coef_%s.bin", sof_ctl, prm.id); % Blob binary file
alsa3_fn = sprintf("%s/ipc3/dcblock/coef_%s.txt", sof_ctl, prm.id); % ALSA CSV format file
blob4_fn = sprintf("%s/ipc4/dcblock/coef_%s.bin", sof_ctl, prm.id); % Blob binary file
alsa4_fn = sprintf("%s/ipc4/dcblock/coef_%s.txt", sof_ctl, prm.id); % ALSA CSV format file

endian = "little";

if isempty(prm.fc)
	R_coeffs = prm.R_coeffs;
else
	channels = 8;
	R = dcblock_rval_calculate(prm.fs, prm.fc);
	R_coeffs = R * ones(1, channels);
end

sof_dcblock_paths(true);

blob8 = sof_dcblock_build_blob(R_coeffs, endian);
blob8_ipc4 = sof_dcblock_build_blob(R_coeffs, endian, 4);

% Generate output files
sof_tplg_write(tplg1_fn, blob8, "DCBLOCK", ...
	       "Exported with script sof_example_dcblock.m", ...
	       "cd tools/tune/dcblock; octave sof_example_dcblock.m");
sof_ucm_blob_write(blob3_fn, blob8);
sof_alsactl_write(alsa3_fn, blob8);

sof_tplg2_write(tplg2_fn, blob8_ipc4, "dcblock_config", ...
		"Exported with script sof_example_dcblock.m" , ...
		"cd tools/tune/dcblock; octave sof_example_dcblock.m");
sof_ucm_blob_write(blob4_fn, blob8_ipc4);
sof_alsactl_write(alsa4_fn, blob8_ipc4);

% Plot Filter's Transfer Function and Step Response
% As an example, plot the graphs of the first coefficient
fs = 48e3;
sof_dcblock_plot_transferfn(R_coeffs(1), fs);
figure
sof_dcblock_plot_stepfn(R_coeffs(1), fs);

sof_dcblock_paths(false);

end

% Finds with iterative search parameter R for given cutoff frequency
function R = dcblock_rval_calculate(fs, fc_target)

if (fc_target / fs < 10 / 48e3 || fc_target / fs > 1000 / 48e3)
	error("Illegal fc_target");
end

h_target = 1 / sqrt(2); % -3.01 dB
R = 0.5;
R_step = 0.005;
sign = 1;
w = 2 * pi * fc_target / fs;
j = sqrt(-1);
z = exp(j * w);

% Iteration 0
h = (1 - z^-1) / (1 - R * z^-1);
err_prev = (h_target - abs(h))^2;
R = R + sign * R_step;

% Do more iterations
for n = 1 : 200
	h = (1 - z^-1) / (1 - R * z^-1);
	err = (h_target - abs(h))^2;
	if (err > err_prev)
		sign = -sign;
		R_step = R_step / 2;
	end
	R = R + sign * R_step;
	err_prev = err;
end

% Sane result?
if R < eps || R > 1 - eps
	error("Calculate of R iteration failed");
end

% Sane high-pass function?
f = [1 fc_target fs/2];
b = [ 1 -1 ]; a = [ 1 -R ];
h = freqz(b, a, f, fs);
h_db = 20*log10(abs(h));
err_hfc = abs(20*log10(1/sqrt(2)) - h_db(2));
if err_hfc > 0.01 || h_db(1) > -10 || h_db(3) < 0 || h_db(3) > 1
	error("Failed high-pass response");
end

end
