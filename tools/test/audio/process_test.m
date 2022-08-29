% process_test - test objective audio quality parameters
%
% process_test(comp, bits_in_list, bits_out_list, fs)

% SPDX-License-Identifier: BSD-3-Clause
% Copyright(c) 2017-2022 Intel Corporation. All rights reserved.
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

function  [n_fail, n_pass, n_na] = process_test(comp, bits_in_list, bits_out_list, fs, fulltest)
	%% Defaults for call parameters
	if nargin < 1
		comp = 'EQIIR';
	end

	if nargin < 2
		bits_in_list = 32;
	end

	if nargin < 3
		bits_out_list = 32;
	end

	if nargin < 4
		fs = 48e3;
	end

	if nargin < 5
		fulltest = 1;
	end

	%% Paths
	t.blobpath = '../../topology/topology1/m4';
	plots = 'plots';
	reports = 'reports';

	%% Defaults for test
	t.comp = comp;                         % Pass component name from func arguments
	t.fmt = 'raw';                         % Can be 'raw' (fast binary) or 'txt' (debug)
	t.iirblob = 'eq_iir_coef_loudness.m4'; % Use loudness type response
	t.firblob = 'eq_fir_coef_loudness.m4'; % Use loudness type response
	t.fs = fs;                             % Sample rate from func arguments
	t.nch = 2;                             % Number of channels
	t.ch = [1 2];                          % Test channel 1 and 2
	t.bits_in = bits_in_list;              % Input word length from func arguments
	t.bits_out = bits_out_list;            % Output word length from func arguments
	t.full_test = fulltest;                % 0 is quick check only, 1 is full test

	%% Show graphics or not. With visible plot windows Octave may freeze if too
	%  many windows are kept open. As workaround setting close windows to
	%  1 shows only flashing windows while the test proceeds. With
	%  visibility set to 0 only console text is seen. The plots are
	%  exported into plots directory in png format and can be viewed from
	%  there.
	t.plot_close_windows = 1;  % Workaround for visible windows if Octave hangs
	t.plot_visible = 'off';    % Use off for batch tests and on for interactive
	t.files_delete = 1;        % Set to 0 to inspect the audio data files

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
				[v(2), g] = g_test(t);
				[v(3), dr] = dr_test(t);
				[v(4), thdnf] = thdnf_test(t);
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

	%% FIXME: get unique string to keep all the incremental logs, like datetime string.
	%% For now bitspersample is differentiator.

	%% Print table with test summary: Gain
	fn = sprintf('%s/g_%s_%d.txt', reports, t.comp, t.bits_in);
	print_val(t.comp, 'Gain (dB)', fn, bits_in_list, bits_out_list, r.g, r.pf);

	%% Print table with test summary: DR
	fn = sprintf('%s/dr_%s_%d.txt', reports, t.comp, t.bits_in);
	print_val(t.comp, 'Dynamic range (dB CCIR-RMS)', fn, bits_in_list, bits_out_list, r.dr, r.pf);

	%% Print table with test summary: THD+N vs. frequency
	fn = sprintf('%s/thdnf_%s_%d.txt', reports, t.comp, t.bits_in);
	print_val(t.comp, 'Worst-case THD+N vs. frequency', fn, bits_in_list, bits_out_list, r.thdnf, r.pf);


	%% Print table with test summary: pass/fail
	fn = sprintf('%s/pf_%s_%d.txt', reports, t.comp, t.bits_in);
	print_pf(t.comp', fn, bits_in_list, bits_out_list, r.pf, 'Fails chirp/gain/DR/THD+N/FR');

	fprintf('\n');
	fprintf('============================================================\n');
	fprintf('Number of passed tests = %d\n', r.n_pass);
	fprintf('Number of failed tests = %d\n', r.n_fail);
	fprintf('Number of non-applicable tests = %d\n', r.n_na);
	fprintf('Number of skipped tests = %d\n', r.n_skipped);
	fprintf('============================================================\n');

	if r.n_fail > 0 || r.n_pass < 1
		fprintf('\nERROR: TEST FAILED!!!\n');
	else
		fprintf('\nTest passed.\n');
	end

	n_fail = r.n_fail;
	n_pass = r.n_pass;
	n_na = r.n_na;
end

%% ------------------------------------------------------------
%% Test execution with help of common functions
%%

function fail = chirp_test(t)
	fprintf('Spectrogram test %d Hz ...\n', t.fs);

	% Create input file
	test = test_defaults(t);
	test = chirp_test_input(test);

	% Run test
	test = test_run_process(test);

	% Analyze
	test = chirp_test_analyze(test);
	test_result_print(t, 'Continuous frequency sweep', 'chirpf', test);

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
	test = test_run_process(test);

	% Measure
	test = g_spec(test, t);
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
	test = test_run_process(test);

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
	test = test_run_process(test);

	% Measure
	test = thdnf_mask(test);
	test = thdnf_test_measure(test);

	% For EQ use the -20dBFS result and ignore possible -1 dBFS fail
	thdnf = max(test.thdnf_low);
	fail = test.fail;
	delete_check(t.files_delete, test.fn_in);
	delete_check(t.files_delete, test.fn_out);

	% Print
	test_result_print(t, 'THD+N ratio vs. frequency', 'THDNF', test);
end

%% Reference: AES17 6.2.3 Frequency response
function fail = fr_test(t)
	test = test_defaults(t);

	% Create input file
	test = fr_test_input(test);

	% Run test
	test = test_run_process(test);

	% Measure
	test = fr_mask(test, t);
	test = fr_test_measure(test);
	fail = test.fail;
	delete_check(t.files_delete, test.fn_in);
	delete_check(t.files_delete, test.fn_out);

	% Print
	test_result_print(t, 'Frequency response', 'FR', test);
end

%% ------------------------------------------------------------
%% Helper functions

function test = thdnf_mask(test)
	min_bits = min(test.bits_in, test.bits_out);
	test.thdnf_mask_f = [50 400 test.f_max];
	test.thdnf_mask_hi = [-40 -50 -50];
end

function test = g_spec(test, prm)
	switch lower(test.comp)
		case 'eq-iir'
			blob = fullfile(prm.blobpath, prm.iirblob);
			h = eq_blob_plot(blob, 'iir', test.fs, test.f, 0);
		case 'eq-fir'
			blob = fullfile(prm.blobpath, prm.firblob);
			h = eq_blob_plot(blob, 'fir', test.fs, test.f, 0);
		otherwise
			test.g_db_expect = zeros(1, test.nch);
			return
	end

	test.g_db_expect = h.m(:, test.ch);
end

function test = fr_mask(test, prm)
	switch lower(test.comp)
		case 'eq-iir'
			blob = fullfile(prm.blobpath, prm.iirblob);
			h = eq_blob_plot(blob, 'iir', test.fs, test.f, 0);
		case 'eq-fir'
			blob = fullfile(prm.blobpath, prm.firblob);
			h = eq_blob_plot(blob, 'fir', test.fs, test.f, 0);
		otherwise
			% Define a generic mask for frequency response, generally
			% all processing at 8 kHz or above should pass, if not
				% or need for tighter criteria define per component other
			% target.
			test.fr_mask_fhi = [20 test.f_max];
			test.fr_mask_flo = [200 400 3500 3600 ];
			for i = 1:test.nch
				test.fr_mask_mhi(:,i) = [ 1 1 ];
				test.fr_mask_mlo(:,i) = [-10 -1 -1 -10];
			end
			return
	end

	% Create mask from theoretical frequency response calculated from decoded
	% response in h and align mask to be relative to 997 Hz response
	i997 = find(test.f > 997, 1, 'first')-1;
	j = 1;
	for channel = test.ch
		m = h.m(:, channel) - h.m(i997, channel);
		test.fr_mask_flo = test.f;
		test.fr_mask_fhi = test.f;
		test.fr_mask_mlo(:,j) = m - test.fr_rp_max_db;
		test.fr_mask_mhi(:,j) = m + test.fr_rp_max_db;
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
	test.plot_thdn_axis = [];
	test.plot_fr_axis = [];
	test.plot_passband_zoom = 0;

	% Test constraints
	test.f_start = 20;
	test.f_end = test.fs * 0.41667; % 20 kHz @ 48 kHz
	test.fu = test.fs * 0.41667;    % 20 kHz @ 48 kHz
	test.f_max = 0.999*t.fs/2;      % Measure up to min. Nyquist frequency
	test.fs1 = test.fs;
	test.fs2 = test.fs;

	% Pass criteria
	test.g_db_tol = 0.1;            % Allow 0.1 dB gain variation
	test.thdnf_max = [];            % Set per component
	test.dr_db_min = 80;            % Min. DR
	test.fr_rp_max_db = 0.5;        % Allow 0.5 dB frequency response ripple
end

function test = test_run_process(test)
	delete_check(1, test.fn_out);
	test = test_run(test);
end

function test_result_print(t, testverbose, testacronym, test)
	tstr = sprintf('%s %s %d-%d %d Hz', ...
			testverbose, t.comp, t.bits_in, t.bits_out, t.fs);

	%% FIXME: get unique string to keep all the incremental logs

	for i = 1:length(test.ph)
		title(test.ph(i), tstr);
	end

	for i = 1:length(test.fh)
		figure(test.fh(i));
		set(test.fh(i), 'visible', test.plot_visible);
		pfn = sprintf('plots/%s_%s_%d_%d_%d_%d.png', ...
				testacronym, t.comp, ...
				t.bits_in, t.bits_out, t.fs, i);
		print(pfn, '-dpng');
	end
end
