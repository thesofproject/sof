function  [n_fail, n_pass, n_na] = process_test(comp, bits_in_list, bits_out_list, fs)

% process_test - test objective audio quality parameters
%
% process_test(comp, bits_in_list, bits_out_list, fs)

% SPDX-License-Identifier: BSD-3-Clause
% Copyright(c) 2017-202 Intel Corporation. All rights reserved.
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

%% Defaults for call parameters
if nargin < 1
	comp = 'EQIIR';
end

if nargin < 2
	bits_in_list = [16 32];
end

if nargin < 3
	bits_out_list = [16 32];
end

if nargin < 4
	fs = 48e3;
end

%% Paths
blobpath = '../../topology/m4';
plots = 'plots';
reports = 'reports';

t.comp = comp;
t.blob = fullfile(blobpath, 'eq_iir_coef_flat.m4');
%t.blob = fullfile(blobpath, 'eq_iir_coef_loudness.m4');
%t.comp = 'EQIIR';
%t.comp = 'EQFIR';
%t.comp = 'volume';
%t.comp = 'DCblock';

%% Defaults for test
t.fmt = 'raw';              % Can be 'raw' (fast binary) or 'txt' (debug)
t.fs = fs;                  % Sample rate from func params
t.nch = 2;                  % Number of channels
t.ch = [1 2];               % Test channel 1
t.bits_in = bits_in_list;   % Input word length from func params
t.bits_out = bits_out_list; % Output word length from func params
t.full_test = 1;            % 0 is quick check only, 1 is full set

%% Show graphics or not. With visible plot windows Octave may freeze if too
%  many windows are kept open. As workaround setting close windows to
%  1 shows only flashing windows while the test proceeds. With
%  visibility set to to 0 only console text is seen. The plots are
%  exported into plots directory in png format and can be viewed from
%  there.
t.plot_close_windows = 0;  % Workaround for visible windows if Octave hangs
t.plot_visible = 'on';     % Use off for batch tests and on for interactive
t.files_delete = 0;        % Set to 0 to inspect the audio data files

%% Prepare
addpath('std_utils');
addpath('test_utils');
addpath('../../tune/eq');
mkdir_check(plots);
mkdir_check(reports);
n_meas = 5;
n_bits_in = length(bits_in_list);
n_bits_out = length(bits_out_list);
r.bits_in_list = bits_in_list;
r.bits_out_list = bits_out_list;
r.g = NaN(n_bits_in, n_bits_out);
r.dr = NaN(n_bits_in, n_bits_out);
r.thdnf = NaN(n_bits_in, n_bits_out);
r.pf = -ones(n_bits_in, n_bits_out, n_meas);
r.n_fail = 0;
r.n_pass = 0;
r.n_skipped = 0;
r.n_na = 0;

%% Loop all modes to test
for b = 1:n_bits_out
	for a = 1:n_bits_in
		v = -ones(n_meas,1); % Set pass/fail test verdict to not executed
		tn = 1;
		t.bits_in = bits_in_list(a);
		t.bits_out = bits_out_list(b);

                v(1) = chirp_test(t);
		if v(1) ~= -1 && t.full_test
			[v(2) g] = g_test(t);
			[v(3) dr] = dr_test(t);
			[v(4) thdnf] = thdnf_test(t);
			v(5) = fr_test(t);

			% TODO: Collect results for all channels, now get worst-case
			r.g(a, b) = g(1);
			r.dr(a, b) = min(dr);
			r.thdnf(a, b) = max(thdnf);
			r.pf(a, b, :) = v;
		end

		%% Done, store pass/fail
		if v(1) ~= -1
			idx = find(v > 0);
			r.n_fail = r.n_fail + length(idx);
			idx = find(v == 0);
			r.n_pass = r.n_pass + length(idx);
			idx = find(v == -1);
			r.n_skipped = r.n_skipped + length(idx);
			idx = find(v == -2);
			r.n_na = r.n_na + length(idx);
		end
	end
end

%% Print table with test summary: Gain
fn = sprintf('%s/g_%s.txt', reports, t.comp);
print_val(t.comp, 'Gain (dB)', fn, bits_in_list, bits_out_list, r.g, r.pf);

%% Print table with test summary: THD+N vs. frequency
fn = sprintf('%s/thdnf_%s.txt', reports, t.comp);
print_val(t.comp, 'Worst-case THD+N vs. frequency', fn, bits_in_list, bits_out_list, r.thdnf, r.pf);

%% Print table with test summary: DR
fn = sprintf('%s/dr_%s.txt', reports, t.comp);
print_val(t.comp, 'Dynamic range (dB CCIR-RMS)', fn, bits_in_list, bits_out_list, r.dr, r.pf);

%% Print table with test summary: pass/fail
fn = 'reports/pf_src.txt';
print_pf(t.comp', fn, bits_in_list, bits_out_list, r.pf);

fprintf('\n');
fprintf('Number of passed tests = %d\n', r.n_pass);
fprintf('Number of failed tests = %d\n', r.n_fail);
fprintf('Number of non-applicable tests = %d\n', r.n_na);
fprintf('Number of skipped tests = %d\n', r.n_skipped);


if r.n_fail > 0 || r.n_pass < 1
	fprintf('\nERROR: TEST FAILED!!!\n');
else
	fprintf('\nTest passed.\n');
end

n_fail = r.n_fail;
n_pass = r.n_pass;
n_na = r.n_na;

%% Done
end


%%
%% Test execution with help of common functions
%%

function fail = chirp_test(t)

fprintf('Spectrogram test %d Hz ...\n', t.fs);

% Create input file
test = test_defaults(t);
test = chirp_test_input(test);

% Run test
test = test_run_process(test, t);

% Analyze
test = chirp_test_analyze(test);
test_result_print(t, 'Continuous frequency sweep', 'chirpf');

% Delete files unless e.g. debugging and need data to run
delete_check(t.files_delete, test.fn_in);
delete_check(t.files_delete, test.fn_out);

fail = test.fail;
end


%% Reference: AES17 6.2.2 Gain
function [fail, g_db] = g_test(t)

test = test_defaults(t);

% Create input file
test = g_test_input(test);

% Run test
test = test_run_process(test, t);

% EQ gain at test frequency
channel = 1;
h = eq_blob_plot(t.blob, 'iir', test.fs, test.f, 0);
test.g_db_expect = h.m(channel);

% Measure
test = g_test_measure(test);

% Get output parameters
fail = test.fail;
g_db = test.g_db;
delete_check(t.files_delete, test.fn_in);
delete_check(t.files_delete, test.fn_out);

end

%% Reference: AES17 6.4.1 Dynamic range
function [fail, dr_db] = dr_test(t)

test = test_defaults(t);

% Create input file
test = dr_test_input(test);

% Run test
test = test_run_process(test, t);

% Measure
test = dr_test_measure(test);

% Get output parameters
fail = test.fail;
dr_db = test.dr_db;
delete_check(t.files_delete, test.fn_in);
delete_check(t.files_delete, test.fn_out);

end

%% Reference: AES17 6.3.2 THD+N ratio vs. frequency
function [fail, thdnf] = thdnf_test(t)

test = test_defaults(t);

% Create input file
test = thdnf_test_input(test);

% Run test
test = test_run_process(test, t);

% Measure
test = thdnf_test_measure(test);

% For EQ use the -20dBFS result and ignore possible -1 dBFS fail
thdnf = max(test.thdnf_low);
if thdnf > test.thdnf_max
  fail = 1;
else
  fail = 0;
end
delete_check(t.files_delete, test.fn_in);
delete_check(t.files_delete, test.fn_out);

% Print
test_result_print(t, 'THD+N ratio vs. frequency', 'THDNF');

end


%% Reference: AES17 6.2.3 Frequency response
function [fail, pm_range_db, range_hz, fr3db_hz] = fr_test(t)

test = test_defaults(t);

% Create input file
test = fr_test_input(test);

% Run test
test = test_run_process(test, t);

% Measure
test = eq_mask(test, t, 0.5);
test = fr_test_measure(test);

fail = test.fail;
pm_range_db = test.rp;
range_hz = [test.f_lo test.f_hi];
fr3db_hz = test.fr3db_hz;
delete_check(t.files_delete, test.fn_in);
delete_check(t.files_delete, test.fn_out);

% Print
test_result_print(t, 'Frequency response', 'FR', test.ph, test.fh);

end

%% Helper functions

% Create mask from theoretical frequency response calculated from coefficients
% and align mask to be relative 997 Hz response
function test = eq_mask(test, t, tol)

if ~strcmp(lower(test.comp), 'eqiir') && ~strcmp(lower(test.comp), 'eqfir')
	return
end

test.fr_mask_f = [];
test.fr_mask_lo = [];
test.fr_mask_hi = [];

j = 1;
for channel = t.ch
	h = eq_blob_plot(t.blob, 'iir', test.fs, test.f, 0);
	i997 = find(test.f > 997, 1, 'first')-1;
	m = h.m(:, channel) - h.m(i997, channel);
	test.fr_mask_f = test.f;
	test.fr_mask_lo(:,j) = m-tol;
	test.fr_mask_hi(:,j) = m+tol;
	j = j + 1;
end

end

function test = test_defaults(t)

test.comp = t.comp;
test.fmt = t.fmt;
test.bits_in = t.bits_in;
test.bits_out = t.bits_out;
test.nch = t.nch;
test.ch = t.ch;
test.fs = t.fs;
test.plot_visible = t.plot_visible;

% Misc
test.quick = 0;
test.att_rec_db = 0;

% Plotting
test.plot_channels_combine = 1;
test.plot_thdn_axis = [10 100e3 -180 -50];
test.plot_fr_axis = [10 100e3 -20 10 ];
test.plot_passband_zoom = 0;

% Test constraints
test.f_start = 20;
test.f_end = test.fs * 0.41667; % 20 kHz @ 48 kHz
test.fu = test.fs * 0.41667;    % 20 kHz @ 48 kHz
test.f_lo = 20;                 % For response reporting, measure from 20 Hz
test.f_hi = 0.999*t.fs/2 ;      % to designed filter upper frequency
test.f_max = 0.999*t.fs/2;      % Measure up to min. Nyquist frequency
test.fs1 = test.fs;
test.fs2 = test.fs;

% Pass criteria
test.g_db_tol = 0.1;            % Allow 0.1 dB gain variation
test.thdnf_max = -50;           % Max. THD+N over frequency for -20 dBFS
test.dr_db_min = 80;            % Min. DR
test.fr_rp_max_db = 0.1;        % Allow 0.1 dB frequency response ripple

% A generic relaxed frequency response mask
test.fr_mask_f = [ 100 400 7000 ];
for i = 1:t.nch
	test.fr_mask_lo(:,i) = [-9 -1 -1 ];
	test.fr_mask_hi(:,i) = [ 1  1  1 ];
end

end

function test = test_run_process(test, t)

switch lower(test.comp)
	case 'eqiir'
		test.ex = './eqiir_run.sh';
	case 'eqfir'
		test.ex = './eqiir_run.sh';
	case 'dcblock'
		test.ex = './dcblock_run.sh';
	case 'volume'
		test.ex = './volume_run.sh';
	otherwise
		error('Unknown component');
end

test.arg = { num2str(test.bits_in) num2str(test.bits_out) ...
	     num2str(test.fs), test.fn_in, test.fn_out };
delete_check(1, test.fn_out);
test = test_run(test);

end

function test_result_print(t, testverbose, testacronym, ph, fh)

tstr = sprintf('%s %s %d-%d %d Hz', ...
	       testverbose, t.comp, t.bits_in, t.bits_out, t.fs);
if nargin > 3
	nph = length(ph);
	for i=1:nph
		title(ph(i), tstr);
	end
else
	title(tstr);
end

if nargin > 4
	nfh = length(fh);
	for i=1:nfh
		figure(fh(i));
		pfn = sprintf('plots/%s_%s_%d_%d_%d_%d.png', ...
			      testacronym, t.comp, ...
			      t.bits_in, t.bits_out, t.fs, i);
		print(pfn, '-dpng');
	end
else
	pfn = sprintf('plots/%s_%s_%d_%d_%d.png', ...
		      testacronym, t.comp, t.bits_in, t.bits_out, t.fs);
	print(pfn, '-dpng');
end
end
