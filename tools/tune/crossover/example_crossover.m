function example_crossover()

addpath ./../common

% Sampling Frequency and Frequency cut-offs for crossover
cr.fs = 48e3;
cr.fc_low = 200;
cr.fc_med = 1000;
cr.fc_high = 3000;

% 2 way crossover, pipeline IDs of sinks are 1 and 2 (IPC3)
% and component output pins 0 and 1 (IPC4)
cr.num_sinks = 2;
cr.sinks = [1 2];
export_crossover(cr);
cr.sinks = [0 1];
export_crossover(cr);

% 3 way crossover, pipeline IDs of sinks are 1 - 3
cr.num_sinks = 3;
cr.sinks = [1 2 3];
export_crossover(cr);
cr.sinks = [0 1 2];
export_crossover(cr);

% 4 way crossover, pipeline IDs of sinks are 1 - 4
cr.num_sinks = 4;
cr.sinks = [1 2 3 4];
export_crossover(cr);
cr.sinks = [0 1 2 3];
export_crossover(cr);

rmpath ./../common

end

function export_crossover(cr)

endian = "little";
tpath1 = '../../topology/topology1/m4/crossover';
tpath2 = '../../topology/topology2/include/components/crossover';
ctlpath3 = '../../ctl/ipc3/crossover';
ctlpath4 = '../../ctl/ipc4/crossover';

str_way = sprintf('%dway', cr.num_sinks);
str_freq = get_str_freq(cr);
str_pid = get_str_pid(cr);

% Set the parameters here
tplg1_fn = sprintf('%s/coef_%s_%s_%s.m4', tpath1, str_way, str_freq, str_pid); % Control Bytes File
tplg2_fn = sprintf('%s/coef_%s_%s_%s.conf', tpath2, str_way, str_freq, str_pid);
% Use those files with sof-ctl to update the component's configuration
blob3_fn = sprintf('%s/coef_%dway.blob', ctlpath3, cr.num_sinks); % Blob binary file
alsa3_fn = sprintf('%s/coef_%dway.txt', ctlpath3, cr.num_sinks); % ALSA CSV format file
blob4_fn = sprintf('%s/coef_%dway.blob', ctlpath4, cr.num_sinks); % Blob binary file
alsa4_fn = sprintf('%s/coef_%dway.txt', ctlpath4, cr.num_sinks); % ALSA CSV format file

% This array is an example on how to assign a buffer from pipeline 1 to output 0,
% buffer from pipeline 2 to output 1, etc...
% Refer to sof/src/include/user/crossover.h for more information on assigning
% buffers to outputs.
assign_sinks = zeros(1, 4);
assign_sinks(1:cr.num_sinks) = cr.sinks;

% Generate zeros, poles and gain for crossover with the given frequencies
switch cr.num_sinks
	case 2
		crossover = crossover_gen_coefs(cr.fs, cr.fc_low); % 2 way crossover
	case 3
		crossover = crossover_gen_coefs(cr.fs, cr.fc_low, cr.fc_med); % 3 way crossover
	case 4
		crossover = crossover_gen_coefs(cr.fs, cr.fc_low, cr.fc_med, cr.fc_high); % 4 way crossover
	otherwise
		error('Illegal number of sinks %d\n', num_sinks);
end

% Convert the [a,b] coefficients to values usable with SOF
crossover_bqs = crossover_coef_quant(crossover.lp, crossover.hp);

% Convert coefficients to sof_crossover_config struct
config = crossover_generate_config(crossover_bqs, cr.num_sinks, assign_sinks);

% Convert struct to binary blob
blob8 = crossover_build_blob(config, endian, 3);
blob8_ipc4 = crossover_build_blob(config, endian, 4);

% Generate output files

mkdir_check(tpath1);
mkdir_check(tpath2);
mkdir_check(ctlpath3);
mkdir_check(ctlpath4);
tplg_write(tplg1_fn, blob8, "CROSSOVER");
tplg2_write(tplg2_fn, blob8_ipc4, "crossover_config", 'Exported Control Bytes');
sof_ucm_blob_write(blob3_fn, blob8);
sof_ucm_blob_write(blob4_fn, blob8_ipc4);
alsactl_write(alsa3_fn, blob8);
alsactl_write(alsa4_fn, blob8_ipc4);

% Plot Magnitude and Phase Response of each sink
crossover_plot_freq(crossover.lp, crossover.hp, cr.fs, cr.num_sinks);

end

% Frequencies part for filename
function str = get_str_freq(cr)

switch cr.num_sinks
	case 2
		str = sprintf('%d_%d', cr.fs, cr.fc_low);
	case 3
		str = sprintf('%d_%d_%d', cr.fs, cr.fc_low, cr.fc_med);
	case 4
		str = sprintf('%d_%d_%d_%d', cr.fs, cr.fc_low, cr.fc_med, cr.fc_high);
end

end

% Pipeline IDs part of filename
function str = get_str_pid(cr)

str = sprintf('%d', cr.sinks(1));
for i = 2:cr.num_sinks
	str = sprintf('%s_%d', str, cr.sinks(i));
end

end

% Check if directory exists, avoid warning print for existing
function mkdir_check(new_dir)

	if ~exist(new_dir, 'dir')
		mkdir(new_dir);
	end

end
