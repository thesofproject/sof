function tdfb_direction_test()

% tdfb_test()
% Inputs
%   None
%
% Outputs
%   None, to be added later when automatic pass/fail is possible to
%   determine. So far only visual check enabled.

% SPDX-License-Identifier: BSD-3-Clause
% Copyright(c) 2020 Intel Corporation. All rights reserved.
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

% General settings
cfg.delete_files = 1;
cfg.do_plots = 1;
cfg.tunepath = '../../tune/tdfb/data';

% Arrays to test. Since the two beams configurations are merge of two designs (pm90deg)
% need to specify a compatible data file identifier for a single beam design (az0el0deg)
array_data_list = {'line2_50mm_az0el0deg_48khz', 'line4_28mm_az0el0deg_48khz', 'circular8_100mm_az0el0deg_48khz'};
tdfb_name_list =  {'', 'line4_28mm_pm90deg_48khz', 'circular8_100mm_pm30deg_48khz'};

%% Prepare
addpath('std_utils');
addpath('test_utils');
addpath('../../tune/tdfb');

for i = 1:length(array_data_list)

        % Get configuration, this needs to match array geometry and rate
        % beam direction can be any, use (0, 0) deg.
	array = array_data_list{i};
	tdfb = tdfb_name_list{i};

	% Rub beapattern test with rotated noise
	config_fn = sprintf('tdfb_coef_%s.mat', array);
	simcap_fn = sprintf('simcap_noiserot_%s.raw', array);
	test_beampattern(cfg, config_fn, simcap_fn, tdfb);

	% Plot estimated direction
	trace_fn = 'tdfb_direction.txt';
	label = 'tdfb_dint';
	comp = 'tdfb';
	inst = '';
	dataidx = [1 2 3 4];
	[data, ts, dt] = trace_parse_tb(trace_fn, comp, inst, label, dataidx);
	ref = 32768^2;
	offs = 20*log10(sqrt(2));
	trig = 10 * bitand(data(:,1), 1);
	flev = 10*log10(data(:,2)/ref) + offs;
	slev = 10*log10(data(:,3)/ref) + offs;
	az_slow = data(:,4) / 2^12 * 180/pi;

	figure
	plot(flev)
	hold on
	plot(slev)
	plot(trig)
	plot(az_slow)
	hold off
	grid on
	xlabel('Time (ms)');
	ylabel('Levels, trigger, angle');
	legend('fast level', 'slow level', 'trigger', 'slow az' );
	title(sprintf('Direction angle %s', array));
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
test.extra_opts='-d 4';
test.trace = 'tdfb_direction.txt';
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
test.fn_out = 'noiserot.raw';

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

function [data, ts, dt] = trace_parse_tb(trace_fn, comp, inst, label, dataidx)
	nmax = 1000000; % Max. more than 15 min every 1 ms
	fh = fopen(trace_fn, 'r');
	dlines = 0;
	nlines = 0;
	ndata = length(dataidx);
	ncols = ndata;
	dtmp = zeros(nmax, ncols);
	ltmp = zeros(1, ncols);
	str = fgets(fh);
	while (str ~= -1)
		nlines = nlines + 1;
		idx = strfind(str, label);
		if isempty(idx)
			str = fgets(fh);
			continue;
		end

		dstr = [ ' ' str(idx + length(label) + 1:end) ' ' ];
		idx = strfind(dstr, ' ');
		for i = 1:ndata
			j = dataidx(i);
			i1 = idx(j) + 1;
			i2 = idx(j+1) - 1;
			nstr = dstr(i1:i2);
			ltmp(i) = str2num(nstr);
		end
		dlines = dlines + 1;
		dtmp(dlines,:) = ltmp;
		str = fgets(fh);
	end

	if dlines == 0
		fprintf(1, 'Read %d lines, no label "%s" found\n', nlines, label);
		error('Failed.');
	end

	dtmp = dtmp(1:dlines,:);
	fprintf(1, 'Found %d lines with label "%s"\n', dlines, label);
	data = dtmp(1:dlines, 1:ncols);

	ts = [];
	dt = [];

end
