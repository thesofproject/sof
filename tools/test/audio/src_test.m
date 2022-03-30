function [n_fail, n_pass, n_na] = src_test(bits_in, bits_out, fs_in_list, fs_out_list)

%%
% src_test - test with SRC test bench objective audio quality parameters
%
% src_test(bits_in, bits_out, fs_in, fs_out)
%
% bits_in  - input word length
% bits_out - output word length
% fs_in    - vector of rates in
% fs_out   - vector of rates out
%
% A default in-out matrix with 32 bits data is tested if the
% parameters are omitted.
%

% SPDX-License-Identifier: BSD-3-Clause
% Copyright(c) 2016 Intel Corporation. All rights reserved.
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

addpath('std_utils');
addpath('test_utils');
addpath('../../tune/src');
mkdir_check('plots');
mkdir_check('reports');

%% Defaults for call parameters
if nargin < 1
	bits_in = 32;
end
if nargin < 2
	bits_out = 32;
end
if nargin < 3
        fs_in_list = [ 8 11.025 12 16 18.9 22.050 24 32 37.8 44.1 48 ...
		       50 64 88.2 96 176.4 192] * 1e3;
end
if nargin < 4
	fs_out_list = [ 8 11.025 12 16 22.05 24 32 44.1 48 ...
		        50 64 88.2 96 176.4 192] * 1e3;
end

%% Generic test pass/fail criteria
%  Note that AAP and AIP are relaxed a bit from THD+N due to inclusion
%  of point Fs/2 to test. The stopband of kaiser FIR is not equiripple
%  and there's sufficient amount of attnuation at higher frequencies to
%  meet THD+N requirement.
t.g_db_tol = 1.1;
t.thdnf_db_max = -80;
t.thdnf_db_16b = -60;
t.dr_db_min = 100;
t.dr_db_16b = 79;
t.aap_db_max = -60;
t.aip_db_max = -60;

%% Defaults for test
t.fmt = 'raw';         % Can be 'raw' (fast binary) or 'txt' (debug)
t.nch = 2;             % Number of channels
t.ch = 0;              % 1..nch. With value 0 test a randomly selected channel.
t.bits_in = bits_in;   % Input word length
t.bits_out = bits_out; % Output word length
t.full_test = 1;       % 0 is quick check only, 1 is full set

%% Show graphics or not. With visible plot windows Octave may freeze if too
%  many windows are kept open. As workaround setting close windows to
%  1 shows only flashing windows while the test proceeds. With
%  visibility set to to 0 only console text is seen. The plots are
%  exported into plots directory in png format and can be viewed from
%  there.
t.plot_close_windows = 1;  % Workaround for visible windows if Octave hangs
t.plot_visible = 'off';    % Use off for batch tests and on for interactive
t.files_delete = 1;        % Set to 0 to inspect the audio data files

%% Init for test loop
n_test = 7; % We have next seven test cases for SRC
n_fsi = length(fs_in_list);
n_fso = length(fs_out_list);
r.fs_in_list = fs_in_list;
r.fs_out_list = fs_out_list;
r.g = NaN(n_fsi, n_fso);
r.dr = NaN(n_fsi, n_fso);
r.fr_db = NaN(n_fsi, n_fso);
r.fr_hz = NaN(n_fsi, n_fso, 2);
r.fr3db_hz = NaN(n_fsi, n_fso);
r.thdnf = NaN(n_fsi, n_fso);
r.aap = NaN(n_fsi, n_fso);
r.aip = NaN(n_fsi, n_fso);
r.pf = NaN(n_fsi, n_fso, n_test);
r.n_fail = 0;
r.n_pass = 0;
r.n_skipped = 0;
r.n_na = 0;
%% Loop all modes to test
for b = 1:n_fso
        for a = 1:n_fsi
                v = -ones(n_test,1); % Set pass/fail test verdict to not executed
                tn = 1;
                t.fs1 = fs_in_list(a);
                t.fs2 = fs_out_list(b);
                v(1) = chirp_test(t);
                if v(1) ~= -1 && t.full_test == 1
                        %% Chirp was processed so this in/out Fs is supported
                        [v(2), r.g(a,b)] = g_test(t);
                        [v(3), r.fr_db(a,b), r.fr_hz(a,b,:), r.fr3db_hz(a,b)] = fr_test(t);
                        [v(4), r.thdnf(a,b)] = thdnf_test(t);
                        [v(5), r.dr(a,b)] = dr_test(t);
                        [v(6), r.aap(a,b)] = aap_test(t);
                        [v(7), r.aip(a,b)] = aip_test(t);
                end
                %% Done, store pass/fail
                r.pf(a, b, :) = v;
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

%% Scale frequencies to kHz
k = 1/1000;

%% Print table with test summary: Gain
fn = 'reports/g_src.txt';
print_val('SRC', 'Gain dB', fn, k * fs_in_list, k * fs_out_list, r.g, r.pf);

%% Print table with test summary: FR
fn = 'reports/fr_src.txt';
print_fr('SRC', fn, k * fs_in_list, k * fs_out_list, r.fr_db, r.fr_hz, r.pf);

%% Print table with test summary: FR
fn = 'reports/fr_src.txt';
print_val('SRC', 'Frequency response -3 dB 0 - X kHz', ...
	  fn, k * fs_in_list, k * fs_out_list, r.fr3db_hz/1e3, r.pf);

%% Print table with test summary: THD+N vs. frequency
fn = 'reports/thdnf_src.txt';
print_val('SRC', 'Worst-case THD+N vs. frequency', ...
	  fn, k * fs_in_list, k * fs_out_list, r.thdnf, r.pf);

%% Print table with test summary: DR
fn = 'reports/dr_src.txt';
print_val('SRC', 'Dynamic range dB (CCIR-RMS)', ...
	  fn, k * fs_in_list, k * fs_out_list, r.dr, r.pf);

%% Print table with test summary: AAP
fn = 'reports/aap_src.txt';
print_val('SRC', 'Attenuation of alias products dB', ...
	  fn, k * fs_in_list, k * fs_out_list, r.aap, r.pf);

%% Print table with test summary: AIP
fn = 'reports/aip_src.txt';
print_val('SRC', 'Attenuation of image products dB', ...
	  fn, k * fs_in_list, k * fs_out_list, r.aip, r.pf);

%% Print table with test summary: pass/fail
fn = 'reports/pf_src.txt';
print_pf('SRC', fn, k * fs_in_list, k * fs_out_list, r.pf, 'chirp/gain/FR/THD+N/DR/AAP/AIP');

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

end


%%
%% Test execution with help of common functions
%%

function [fail, g_db] = g_test(t)

%% Reference: AES17 6.2.2 Gain
test = test_defaults_src(t);
prm = src_param(t.fs1, t.fs2, test.coef_bits);
test.fu = prm.c_pb*min(t.fs1,t.fs2);
test.g_db_tol = t.g_db_tol;
if test.fs1 == test.fs2
	test.g_db_expect = 0;
else
	test.g_db_expect = prm.gain;
end

%% Create input file
test = g_test_input(test);

%% Run test
test = test_run_src(test, t);

%% Measure
test.fs = t.fs2;
test = g_test_measure(test);

%% Get output parameters
fail = test.fail;
g_db = test.g_db;
delete_check(t.files_delete, test.fn_in);
delete_check(t.files_delete, test.fn_out);

end

function [fail, pm_range_db, range_hz, fr3db_hz] = fr_test(t)

%% Reference: AES17 6.2.3 Frequency response
test = test_defaults_src(t);
prm = src_param(t.fs1, t.fs2, test.coef_bits);

test.fr_rp_max_db = prm.rp_tot;                % Max. ripple +/- dB allowed
test.fr_lo = 20;                               % Ripple measure from 20 Hz
test.fr_hi = 0.99 * min(t.fs1,t.fs2)*prm.c_pb; % up to Nyquist frequency
test.f_max = 0.99 * min(t.fs1/2, t.fs2/2);     % Measure up to Nyquist frequency

%% Create input file
test = fr_test_input(test);

%% Run test
test = test_run_src(test, t);

%% Measure
test.fs = t.fs2;
test = fr_test_measure(test);

fail = test.fail;
pm_range_db = test.rp;
range_hz = [test.fr_lo test.fr_hi];
fr3db_hz = test.fr3db_hz;
delete_check(t.files_delete, test.fn_in);
delete_check(t.files_delete, test.fn_out);

%% Print
src_test_result_print(t, 'Frequency response', 'FR', test.ph);

end

function [fail, thdnf] = thdnf_test(t)

%% Reference: AES17 6.3.2 THD+N ratio vs. frequency
test = test_defaults_src(t);
if test.bits_in > 16
	test.thdnf_max = t.thdnf_db_max;
else
	test.thdnf_max = t.thdnf_db_16b;
end

prm = src_param(t.fs1, t.fs2, test.coef_bits);
test.f_start = 20;
test.f_end = prm.c_pb*min(t.fs1, t.fs2);
test.fu = prm.c_pb*min(t.fs1, t.fs2);
%test.f_end = 0.4535*min(t.fs1, t.fs2);
%test.fu = 0.4535*min(t.fs1, t.fs2);

%% Create input file
test = thdnf_test_input(test);

%% Run test
test = test_run_src(test, t);

%% Measure
test.fs = t.fs2;
test = thdnf_test_measure(test);
thdnf = max(max(test.thdnf));
fail = test.fail;
delete_check(t.files_delete, test.fn_in);
delete_check(t.files_delete, test.fn_out);

%% Print
src_test_result_print(t, 'THD+N ratio vs. frequency', 'THDNF');

end

function [fail, dr_db] = dr_test(t)

%% Reference: AES17 6.4.1 Dynamic range
test = test_defaults_src(t);
if test.bits_in > 16
	test.dr_db_min = t.dr_db_min;
else
	test.dr_db_min = t.dr_db_16b;
end

%% Create input file
test = dr_test_input(test);

%% Run test
test = test_run_src(test, t);

%% Measure
test.fs = t.fs2;
test = dr_test_measure(test);

%% Get output parameters
fail = test.fail;
dr_db = test.dr_db;
delete_check(t.files_delete, test.fn_in);
delete_check(t.files_delete, test.fn_out);

end


function [fail, aap_db] = aap_test(t)

%% Reference: AES17 6.6.6 Attenuation of alias products
test = test_defaults_src(t);
test.aap_max = t.aap_db_max;

if t.fs1 <= t.fs2
        %% Internal rate must be lower than input
        fail = -2;
        aap_db = NaN;
        return;
end

test.f_start = 0.5*t.fs1;
test.f_end = 0.5*t.fs2;

%% Create input file
test = aap_test_input(test);

%% Run test
test = test_run_src(test, t);

%% Measure
test.fs = t.fs2;
test = aap_test_measure(test);
aap_db = test.aap;
fail = test.fail;
delete_check(t.files_delete, test.fn_in);
delete_check(t.files_delete, test.fn_out);

%% Print
src_test_result_print(t, 'Attenuation of alias products', 'AAP');

end

function [fail, aip_db] = aip_test(t)

%% Reference: AES17 6.6.7 Attenuation of image products
test = test_defaults_src(t);
test.aip_max = t.aip_db_max;
if t.fs1 >= t.fs2
        %% Internal rate must be higher than input
        fail = -2;
        aip_db = NaN;
        return;
%%
end

%% Create input file
test.f_end = t.fs1/2;
test = aip_test_input(test);

%% Run test
test = test_run_src(test, t);

%% Measure
test.fs = t.fs2;
test = aip_test_measure(test);
aip_db = test.aip;
fail = test.fail;
delete_check(t.files_delete, test.fn_in);
delete_check(t.files_delete, test.fn_out);

%% Print
src_test_result_print(t, 'Attenuation of image products', 'AIP');

end

%% Chirp spectrogram test. This is a visual check only. Aliasing is visible
%  in the plot as additional freqiencies than main linear up sweep. The aliasing
%  can be a line, few lines, or lattice pattern depending the SRC conversion
%  to test. The main sweep line should be steady level and extend from near
%  zero frequency to near Nyquist (Fs/2).

function fail = chirp_test(t)

fprintf('Spectrogram test %d -> %d ...\n', t.fs1, t.fs2);

%% Create input file
test = test_defaults_src(t);
test = chirp_test_input(test);

%% Run test
test = test_run_src(test, t);

%% Analyze
test.fs = t.fs2;
test = chirp_test_analyze(test);
src_test_result_print(t, 'Chirp', 'chirpf');

% Delete files unless e.g. debugging and need data to run
delete_check(t.files_delete, test.fn_in);
delete_check(t.files_delete, test.fn_out);

fail = test.fail;
end


%% Some SRC test specific utility functions
%%

function test = test_defaults_src(t)
test.comp = 'src';
test.fmt = t.fmt;
test.bits_in = t.bits_in;
test.bits_out = t.bits_out;
test.nch = t.nch;
test.ch = t.ch;
test.fs = t.fs1;
test.fs1 = t.fs1;
test.fs2 = t.fs2;
test.coef_bits = 24; % No need to use actual word length in test
test.att_rec_db = 0; % Not used in simulation test
test.quick = 0;      % Test speed is no issue in simulation
test.plot_visible = t.plot_visible;
test.plot_channels_combine = 0;
test.plot_passband_zoom = 1;
test.plot_fr_axis = [10 100e3 -4 1];
test.plot_thdn_axis = [10 100e3 -140 -59];
test.fr_mask_flo = [];
test.fr_mask_fhi = [];
test.fr_mask_mlo = [];
test.fr_mask_mhi = [];
test.thdnf_mask_f = [];
test.thdnf_mask_hi = [];
test.thdnf_max = [];
end

function test = test_run_src(test, t)
test.fs_in = test.fs1;
test.fs_out = test.fs2;
delete_check(1, test.fn_out);
test = test_run(test);
end

function src_test_result_print(t, testverbose, testacronym, ph)
tstr = sprintf('%s SRC %d, %d', testverbose, t.fs1, t.fs2);
if nargin > 3
        title(ph, tstr);
else
        title(tstr);
end
pfn = sprintf('plots/%s_src_%d_%d.png', testacronym, t.fs1, t.fs2);
% The print command caused a strange error with __osmesa_print__
% so disable it for now until solved.
%print(pfn, '-dpng');
if t.plot_close_windows
	close all;
end
end
