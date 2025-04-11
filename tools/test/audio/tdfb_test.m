function tdfb_test()

% tdfb_test()
% Inputs
%   None
%
% Outputs
%   None, to be added later when automatic pass/fail is possible to
%   determine. So far only visual check enabled.

% SPDX-License-Identifier: BSD-3-Clause
% Copyright(c) 2020-2025 Intel Corporation.
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

% General settings
cfg.delete_files = 1;
cfg.do_plots = 1;
cfg.tunepath = '../../../src/audio/tdfb/tune/data';

% Arrays to test. Since the two beams configurations are merge of two designs (pm90deg)
% need to specify a compatible data file identifier for a single beam design (az0el0deg)
array_data_list = {'line2_50mm_az0el0deg_48khz'};
tdfb_name_list =  {''};

%% Prepare
addpath('std_utils');
addpath('test_utils');
addpath('../../../src/audio/tdfb/tune');

for i = 1:length(array_data_list)

	%% Beam pattern test
        % Get configuration, this needs to match array geometry and rate
        % beam direction can be any, use (0, 0) deg.
	array = array_data_list{i};
	tdfb = tdfb_name_list{i};
	config_fn = sprintf('tdfb_coef_%s.mat', array);
	simcap_fn = sprintf('simcap_sinerot_%s.raw', array);
	test_beampattern(cfg, config_fn, simcap_fn, tdfb);

	%% Diffuse noise test
	simcap_fn = sprintf('simcap_diffuse_%s.raw', array);
	desc = 'Diffuse field noise';
	[dfin_dbfs, dfout_dbfs, dfd_db] = test_noise_suppression(cfg, config_fn, simcap_fn, desc, tdfb);

	%% Random noise
	simcap_fn = sprintf('simcap_random_%s.raw', array);
	desc = 'Random noise';
	[rnin_dbfs, rnout_dbfs, drn_db] = test_noise_suppression(cfg, config_fn, simcap_fn, desc, tdfb);

	%% Results
	fprintf(1, '\n');
	print_result('Diffuse field input level ', 'dBFS', dfin_dbfs);
	print_result('Diffuse field output level', 'dBFS', dfout_dbfs);
	print_result('Diffuse field level delta ', 'dB', dfd_db);
	print_result('Random noise input level  ', 'dBFS', rnin_dbfs);
	print_result('Random noise output level ', 'dBFS', rnout_dbfs);
	print_result('Random noise level delta  ', 'dB', drn_db);

end

end

function test = test_defaults(bf, arrayid)

test.comp = 'tdfb';
test.array = '';
test.bits = 16;
test.fs = bf.fs;
test.fmt = 'raw';
test.nch_in = max(bf.input_channel_select) + 1;
test.nch_out = bf.num_output_channels;
test.ch_in = 1:test.nch_in;
test.ch_out = 1:test.nch_out;
test.extra_opts='-s tdfb_enable.sh';
if length(arrayid)
	test.comp = sprintf('tdfb_%s', arrayid);
end

end

function test = test_run_comp(test)

delete_check(1, test.fn_out);
test = test_run(test);

end

function [ldb, az] = sinerot_dbfs(x, bf)

az = bf.sinerot_az_start:bf.sinerot_az_step:bf.sinerot_az_stop;
nt = length(az);
tn = floor(bf.sinerot_t * bf.fs);
sx = size(x);
ldb = zeros(nt, sx(2));
for i = 1:nt
	ts = (i - 1) * bf.sinerot_t;
	i1 = floor(ts * bf.fs + 1);
	i2 = min(i1 + tn - 1, sx(1));
	ldb(i, :) = level_dbfs(x(i1:i2, :));
end

end

%% Beam pattern test

function test_beampattern(cfg, config_fn, simcap_fn, tdfb);

fn = fullfile(cfg.tunepath, config_fn);
if exist(fn, 'file')
	load(fn);
else
	fprintf(1, 'Array configuration file %s does not exist.\n', config_fn);
	fprintf(1, 'Please run the script example_line_array in tools/tune/tdfb directory.\n');
	error('Stopping.');
end

% Create input file
test = test_defaults(bf, tdfb);
test.fn_in = fullfile(cfg.tunepath, simcap_fn);
test.fn_out = 'sinerot.raw';

% Run test
test = test_run_comp(test);

% Load simulation output data
test.nch = test.nch_in;
test.ch = test.ch_in;
x = load_test_input(test);
sx = size(x);
test.nch = test.nch_out;
test.ch = test.ch_out;
y = load_test_output(test);
sy = size(y);
delete_check(cfg.delete_files, test.fn_out);
[rotx_dbfs, az] = sinerot_dbfs(x, bf);
[roty_dbfs, az] = sinerot_dbfs(y, bf);

% Do plots
if cfg.do_plots
	offset = 20*log10(bf.sinerot_a);
	figure
	plot(az, roty_dbfs - offset, '-', az, rotx_dbfs - offset, '--');
	grid on
	xlabel('Azimuth angle (deg)');
	ylabel('Magnitude (dB)');
	ch_legend_help(sy(2));
	tstr = sprintf('Beam pattern %d Hz %s', bf.sinerot_f, bf.array_id);
	title(tstr, 'Interpreter','none');

	figure
	ldb = roty_dbfs - offset;
	llin = 10.^(ldb/20);
	az_rad = az * pi/180;
	if exist('OCTAVE_VERSION', 'builtin')
		polar(az_rad, llin);
	else
		polarplot(az_rad, llin);
	end
	ch_legend_help(sy(2));
	title(tstr, 'Interpreter','none');
end

end

%% Noise suppression test

function [x_dbfs, y_dbfs, delta_db] = test_noise_suppression(cfg, config_fn, simcap_fn, desc, arrayid)

load(fullfile(cfg.tunepath, config_fn));

% Create input file
test = test_defaults(bf, arrayid);
fn_in = fullfile(cfg.tunepath, simcap_fn);
fn_out = 'noise_out.raw';

% Run test
test.fn_in = fn_in;
test.fn_out = fn_out;
test = test_run_comp(test);

% Load simulation input data
test.nch = test.nch_in;
test.ch = test.ch_in;
x = load_test_input(test);
sx = size(x);
test.nch = test.nch_out;
test.ch = test.ch_out;
y = load_test_output(test);
sy = size(y);
delete_check(cfg.delete_files, test.fn_out);
x_dbfs = level_dbfs(x);
y_dbfs = level_dbfs(y);
delta_db = mean(x_dbfs) - y_dbfs;

if cfg.do_plots
	tstr = sprintf('%s %s', desc, bf.array_id);
	figure;
	plot(x(:,1));
	hold on
	plot(y(:,1));
	hold off
	grid on;
	legend('ch1 in','ch1 out');
	title(tstr);
end

end

%% Print results table line

function print_result(desc, unit, values)

fprintf(1, "%s,", desc);
for v = values
	fprintf(1, '%6.1f, ', v);
end
fprintf(1, "%s\n", unit);

end

function ch_legend_help(nch)

switch nch
	case 1
		legend('ch1 out');
	case 2
		legend('ch1 out', 'ch2 out');
	case 3
		legend('ch1 out', 'ch2 out', 'ch3 out');
	case 4
		legend('ch1 out', 'ch2 out', 'ch3 out', 'ch4 out');
end

end
