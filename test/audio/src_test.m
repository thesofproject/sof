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
% paremeters are omitted.
%

%%
% Copyright (c) 2016, Intel Corporation
% All rights reserved.
%
% Redistribution and use in source and binary forms, with or without
% modification, are permitted provided that the following conditions are met:
%   * Redistributions of source code must retain the above copyright
%     notice, this list of conditions and the following disclaimer.
%   * Redistributions in binary form must reproduce the above copyright
%     notice, this list of conditions and the following disclaimer in the
%     documentation and/or other materials provided with the distribution.
%   * Neither the name of the Intel Corporation nor the
%     names of its contributors may be used to endorse or promote products
%     derived from this software without specific prior written permission.
%
% THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
% AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
% IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
% ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
% LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
% CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
% SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
% INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
% CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
% ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
% POSSIBILITY OF SUCH DAMAGE.
%
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
%

addpath('std_utils');
addpath('test_utils');
addpath('../tune/src');
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
        fs_in_list = [8e3 11025 12e3 16e3 18900 22050 24e3 32e3 44100 48e3 ...
			  64e3 88.2e3 96e3 176400 192e3];
end
if nargin < 4
	fs_out_list = [8e3 11025 12e3 16e3 18900 22050 24e3 32e3 44100 48e3];
end

%% Generic test pass/fail criteria
%  Note that AAP and AIP are relaxed a bit from THD+N due to inclusion
%  of point Fs/2 to test. The stopband of kaiser FIR is not equiripple
%  and there's sufficient amount of attnuation at higher frequencies to
%  meet THD+N requirement.
t.g_db_tol = 0.1;
t.thdnf_db_max = -80;
t.dr_db_min = 100;
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
t.close_plot_windows = 0;  % Workaround for visible windows if Octave hangs
t.visible = 'off';         % Use off for batch tests and on for interactive
t.delete = 1;              % Set to 0 to inspect the audio data files

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

%% Print table with test summary: Gain
fn = 'reports/g_src.txt';
print_val(fn, fs_in_list, fs_out_list, r.g, r.pf, 'Gain dB');

%% Print table with test summary: FR
fn = 'reports/fr_src.txt';
print_fr(fn, fs_in_list, fs_out_list, r.fr_db, r.fr_hz, r.pf);

%% Print table with test summary: FR
fn = 'reports/fr_src.txt';
print_val(fn, fs_in_list, fs_out_list, r.fr3db_hz/1e3, r.pf, ...
        'Frequency response -3 dB 0 - X kHz');

%% Print table with test summary: THD+N vs. frequency
fn = 'reports/thdnf_src.txt';
print_val(fn, fs_in_list, fs_out_list, r.thdnf, r.pf, ...
        'Worst-case THD+N vs. frequency');

%% Print table with test summary: DR
fn = 'reports/dr_src.txt';
print_val(fn, fs_in_list, fs_out_list, r.dr, r.pf, ...
        'Dynamic range dB (CCIR-RMS)');

%% Print table with test summary: AAP
fn = 'reports/aap_src.txt';
print_val(fn, fs_in_list, fs_out_list, r.aap, r.pf, ...
        'Attenuation of alias products dB');

%% Print table with test summary: AIP
fn = 'reports/aip_src.txt';
print_val(fn, fs_in_list, fs_out_list, r.aip, r.pf, ...
        'Attenuation of image products dB');


%% Print table with test summary: pass/fail
fn = 'reports/pf_src.txt';
print_pf(fn, fs_in_list, fs_out_list, r.pf);

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
test.g_db_expect = 0;

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
delete_check(t.delete, test.fn_in);
delete_check(t.delete, test.fn_out);

end

function [fail, pm_range_db, range_hz, fr3db_hz] = fr_test(t)

%% Reference: AES17 6.2.3 Frequency response
test = test_defaults_src(t);
prm = src_param(t.fs1, t.fs2, test.coef_bits);

test.rp_max = prm.rp_tot;      % Max. ripple +/- dB allowed
test.f_lo = 20;                % For response reporting, measure from 20 Hz
test.f_hi = min(t.fs1,t.fs2)*prm.c_pb;   % to designed filter upper frequency
test.f_max = min(t.fs1/2, t.fs2/2); % Measure up to Nyquist frequency

%% Create input file
test = fr_test_input(test);

%% Run test
test = test_run_src(test, t);

%% Measure
test.fs = t.fs2;
test = fr_test_measure(test);

fail = test.fail;
pm_range_db = test.rp;
range_hz = [test.f_lo test.f_hi];
fr3db_hz = test.fr3db_hz;
delete_check(t.delete, test.fn_in);
delete_check(t.delete, test.fn_out);

%% Print
src_test_result_print(t, 'Frequency response', 'FR', test.ph);

end

function [fail, thdnf] = thdnf_test(t)

%% Reference: AES17 6.3.2 THD+N ratio vs. frequency
test = test_defaults_src(t);

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
test.thdnf_max = t.thdnf_db_max;
test = thdnf_test_measure(test);
thdnf = max(max(test.thdnf));
fail = test.fail;
delete_check(t.delete, test.fn_in);
delete_check(t.delete, test.fn_out);

%% Print
src_test_result_print(t, 'THD+N ratio vs. frequency', 'THDNF');

end

function [fail, dr_db] = dr_test(t)

%% Reference: AES17 6.4.1 Dynamic range
test = test_defaults_src(t);
test.dr_db_min = t.dr_db_min;

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
delete_check(t.delete, test.fn_in);
delete_check(t.delete, test.fn_out);

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
delete_check(t.delete, test.fn_in);
delete_check(t.delete, test.fn_out);

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
delete_check(t.delete, test.fn_in);
delete_check(t.delete, test.fn_out);

%% Print
src_test_result_print(t, 'Attenuation of image products', 'AIP');

end

%% Chirp spectrogram test. This is a visual check only. Aliasing is visible
%  in the plot as additional freqiencies than main linear up sweep. The aliasing
%  can be a line, few lines, or lattice pattern depending the SRC conversion
%  to test. The main sweep line should be steady level and extend from near
%  zero freqeuncy to near Nyquist (Fs/2).

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
delete_check(t.delete, test.fn_in);
delete_check(t.delete, test.fn_out);

fail = test.fail;
end


%% Some SRC test specific utility functions
%%

function test = test_defaults_src(t)
test.fmt = t.fmt;
test.bits_in = t.bits_in;
test.bits_out = t.bits_out;
test.nch = t.nch;
test.ch = t.ch;
test.fs = t.fs1;
test.fs1 = t.fs1;
test.fs2 = t.fs2;
test.mask_f = [];
test.mask_lo = [];
test.mask_hi = [];
test.coef_bits = 24; % No need to use actual word length in test
test.visible = t.visible;
end

function test = test_run_src(test, t)
test.ex = './src_run.sh';
test.arg = { num2str(t.bits_in), num2str(t.bits_out), num2str(t.fs1), num2str(t.fs2), test.fn_in, test.fn_out};
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
if t.close_plot_windows
	close all;
end
end

%% The next are results printing functions

function  print_val(fn, fs_in_list, fs_out_list, val, pf, valstr)
n_fsi = length(fs_in_list);
n_fso = length(fs_out_list);
fh = fopen(fn,'w');
fprintf(fh,'\n');
fprintf(fh,'SRC test result: %s\n', valstr);
fprintf(fh,'%8s, ', 'in \ out');
for a = 1:n_fso-1
        fprintf(fh,'%8.1f, ', fs_out_list(a)/1e3);
end
fprintf(fh,'%8.1f', fs_out_list(n_fso)/1e3);
fprintf(fh,'\n');
for a = 1:n_fsi
        fprintf(fh,'%8.1f, ', fs_in_list(a)/1e3);
	for b = 1:n_fso
                if pf(a,b,1) < 0
                        cstr = 'x';
                else
                        if isnan(val(a,b))
                                cstr = '-';
                        else
                                cstr = sprintf('%8.2f', val(a,b));
                        end
                end
                if b < n_fso
                        fprintf(fh,'%8s, ', cstr);
                else
                        fprintf(fh,'%8s', cstr);
                end
        end
        fprintf(fh,'\n');
end
fclose(fh);
type(fn);
end

function print_fr(fn, fs_in_list, fs_out_list, fr_db, fr_hz, pf)
n_fsi = length(fs_in_list);
n_fso = length(fs_out_list);
fh = fopen(fn,'w');
fprintf(fh,'\n');
fprintf(fh,'SRC test result: Frequency response +/- X.XX dB (YY.Y kHz) \n');
fprintf(fh,'%8s, ', 'in \ out');
for a = 1:n_fso-1
        fprintf(fh,'%12.1f, ', fs_out_list(a)/1e3);
end
fprintf(fh,'%12.1f', fs_out_list(n_fso)/1e3);
fprintf(fh,'\n');
for a = 1:n_fsi
        fprintf(fh,'%8.1f, ', fs_in_list(a)/1e3);
	for b = 1:n_fso
                if pf(a,b,1) < 0
                        cstr = 'x';
                else
                        cstr = sprintf('%4.2f (%4.1f)', fr_db(a,b), fr_hz(a,b,2)/1e3);
                end
                if b < n_fso
                        fprintf(fh,'%12s, ', cstr);
                else
                        fprintf(fh,'%12s', cstr);
                end
        end
        fprintf(fh,'\n');
end
fclose(fh);
type(fn);
end

function print_fr3db(fn, fs_in_list, fs_out_list, fr3db_hz, pf)
n_fsi = length(fs_in_list);
n_fso = length(fs_out_list);
fh = fopen(fn,'w');
fprintf(fh,'\n');
fprintf(fh,'SRC test result: Frequency response -3dB 0 - X.XX kHz\n');
fprintf(fh,'%8s, ', 'in \ out');
for a = 1:n_fso-1
        fprintf(fh,'%8.1f, ', fs_out_list(a)/1e3);
end
fprintf(fh,'%8.1f', fs_out_list(n_fso)/1e3);
fprintf(fh,'\n');
for a = 1:n_fsi
        fprintf(fh,'%8.1f, ', fs_in_list(a)/1e3);
	for b = 1:n_fso
                if pf(a,b,1) < 0
                        cstr = 'x';
                else
                        cstr = sprintf('%4.1f', fr3db_hz(a,b)/1e3);
                end
                if b < n_fso
                        fprintf(fh,'%8s, ', cstr);
                else
                        fprintf(fh,'%8s', cstr);
                end
        end
        fprintf(fh,'\n');
end
fclose(fh);
type(fn);
end


function print_pf(fn, fs_in_list, fs_out_list, pf)
n_fsi = length(fs_in_list);
n_fso = length(fs_out_list);
fh = fopen(fn,'w');
fprintf(fh,'\n');
fprintf(fh,'SRC test result: Fails\n');
fprintf(fh,'%8s, ', 'in \ out');
for a = 1:n_fso-1
        fprintf(fh,'%14.1f, ', fs_out_list(a)/1e3);
end
fprintf(fh,'%14.1f', fs_out_list(n_fso)/1e3);
fprintf(fh,'\n');
spf = size(pf);
npf = spf(3);
for a = 1:n_fsi
        fprintf(fh,'%8.1f, ', fs_in_list(a)/1e3);
	for b = 1:n_fso
                if pf(a,b,1) < 0
                        cstr = 'x';
                else
                        cstr = sprintf('%d', pf(a,b,1));
                        for n=2:npf
                                if pf(a,b,n) < 0
                                        cstr = sprintf('%s/x', cstr);
                                else
                                        cstr = sprintf('%s/%d', cstr,pf(a,b,n));
                                end
                        end
                end
                if b < n_fso
                        fprintf(fh,'%14s, ', cstr);
                else
                        fprintf(fh,'%14s', cstr);
                end
        end
        fprintf(fh,'\n');
end
fclose(fh);
type(fn);
end
