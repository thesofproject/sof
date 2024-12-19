function sof_example_multiband_drc()

rz1 = [0.09 0.16 0.42 0.98];

prm.name = "default";
prm.sample_rate = 48000;
prm.enable_emp_deemp = 1;
prm.stage_gain = 0.01;
prm.stage_ratio = 2
prm.num_bands = 3;
prm.enable_bands     = [     1      2      3      0 ];
prm.band_lower_freq  = [     0   2000   8000  16000 ];
prm.threshold        = [   -30    -30    -30    -30 ];
prm.knee             = [    20     20     20     20 ];
prm.ratio            = [    10     10     10     10 ];
prm.attack           = [ 0.003  0.003  0.003  0.003 ];
prm.release          = [   0.2    0.2    0.2    0.2 ];
prm.pre_delay        = [ 0.006  0.006  0.006  0.006 ];
prm.release_spacing  = [     5      5      5      5 ];
prm.post_gain        = [     0      0      0      0 ];
prm.release_zone     = [   rz1'   rz1'   rz1'   rz1'];
export_multiband_drc(prm)

prm.name = "passthrough";
prm.sample_rate = 48000;
prm.enable_emp_deemp = 0;
prm.num_bands = 2;
prm.enable_bands = [0 0 0 0];
export_multiband_drc(prm)

end

function export_multiband_drc(prm)

sof_multiband_drc_paths(true);

% Set the parameters here
sof_tools = '../../../../tools';
tplg1_fn = sprintf("%s/topology/topology1/m4/multiband_drc_coef_%s.m4", sof_tools, prm.name); % Control Bytes File
tplg2_fn = sprintf("%s/topology/topology2/include/components/multiband_drc/%s.conf", sof_tools, prm.name); % Control Bytes File
% Use those files with sof-ctl to update the component's configuration
blob3_fn = sprintf("%s/ctl/ipc3/multiband_drc/%s.blob", sof_tools, prm.name); % Blob binary file
alsa3_fn = sprintf("%s/ctl/ipc3/multiband_drc/%s.txt", sof_tools, prm.name); % ALSA CSV format file
blob4_fn = sprintf("%s/ctl/ipc4/multiband_drc/%s.blob", sof_tools, prm.name); % Blob binary file
alsa4_fn = sprintf("%s/ctl/ipc4/multiband_drc/%s.txt", sof_tools, prm.name); % ALSA CSV format file

endian = "little";

sample_rate = prm.sample_rate;
nyquist = sample_rate / 2;

% Number of bands, valid values: 1,2,3,4
num_bands = prm.num_bands;

% 1 to enable Emphasis/Deemphasis filter, 0 to disable it
enable_emp_deemp = prm.enable_emp_deemp;

% The parameters of Emphasis IIR filter
% stage_gain: The gain of each emphasis filter stage
iir_params.stage_gain = prm.stage_gain;
% stage_ratio: The frequency ratio for each emphasis filter stage to the
%              previous stage
iir_params.stage_ratio = prm.stage_ratio;
% anchor: The frequency of the first emphasis filter, in normalized frequency
%         (in [0, 1], relative to half of the sample rate)
iir_params.anchor = 15000 / nyquist;

% The parameters of the DRC compressor
%   enabled: 1 to enable the compressor, 0 to disable it
%   threshold: The value above which the compression starts, in dB
%   knee: The value above which the knee region starts, in dB
%   ratio: The input/output dB ratio after the knee region
%   attack: The time to reduce the gain by 10dB, in seconds
%   release: The time to increase the gain by 10dB, in seconds
%   pre_delay: The lookahead time for the compressor, in seconds
%   release_zone[4]: The adaptive release curve parameters
%   release_spacing: The value of spacing per frame while releasing, in dB
%   post_gain: The static boost value in output, in dB
%   band_lower_freq: The lower frequency of the band, in normalized frequency
%                    (in [0, 1], relative to half of the sample rate)

for i = 1:4
	% Band n DRC parameter, (only valid if num_bands >= i)
	drc_params(i).enabled = prm.enable_bands(i);
	drc_params(i).threshold = prm.threshold(i);
	drc_params(i).knee = prm.knee(i);
	drc_params(i).ratio = prm.ratio(i);
	drc_params(i).attack = prm.attack(i);
	drc_params(i).release = prm.release(i);
	drc_params(i).pre_delay = prm.pre_delay(i);
	drc_params(i).release_zone = prm.release_zone(:, i)';
	drc_params(i).release_spacing = prm.release_spacing(i);
	drc_params(i).post_gain = prm.post_gain(i);
	drc_params(i).band_lower_freq = prm.band_lower_freq(i) / nyquist;
end

% Generate Emphasis/Deemphasis IIR filter quantized coefs struct from parameters

[emp_coefs, deemp_coefs] = sof_iir_gen_quant_coefs(iir_params, sample_rate, enable_emp_deemp);

% Generate Crossover quantized coefs struct from parameters
crossover_coefs = sof_crossover_gen_quant_coefs(num_bands, sample_rate, ...
						drc_params(2).band_lower_freq, ...
						drc_params(3).band_lower_freq, ...
						drc_params(4).band_lower_freq);

% Generate DRC quantized coefs struct from parameters
drc_coefs = sof_drc_gen_quant_coefs(num_bands, sample_rate, drc_params);

% Generate output files

% Convert quantized coefs structs to binary blob
blob8 = sof_multiband_drc_build_blob(num_bands, enable_emp_deemp, emp_coefs, ...
				     deemp_coefs, crossover_coefs, drc_coefs, ...
				     endian, 3);
blob8_ipc4 = sof_multiband_drc_build_blob(num_bands, enable_emp_deemp, emp_coefs, ...
					  deemp_coefs, crossover_coefs, drc_coefs, ...
					  endian, 4);

sof_tplg_write(tplg1_fn, blob8, "MULTIBAND_DRC");
sof_tplg2_write(tplg2_fn, blob8_ipc4, "multiband_drc_config", "Exported with script sof_example_multiband_drc.m");
sof_ucm_blob_write(blob3_fn, blob8);
sof_alsactl_write(alsa3_fn, blob8);
sof_ucm_blob_write(blob4_fn, blob8_ipc4);
sof_alsactl_write(alsa4_fn, blob8_ipc4);

sof_multiband_drc_paths(false);

end
