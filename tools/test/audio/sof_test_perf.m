%%
% sof_test_perf - Test audio objective quality of remote SOF device
%
% [fail, pass] = sof_test_perf(cfgfn)
%
% Inputs
%   id    - a short string to identify test case
%   cfgfn - configuration file for test case
%
% Outputs
%   fail - number of failed tests
%   pass - number of passed tests
%

% SPDX-License-Identifier: BSD-3-Clause
% Copyright(c) 2019 Intel Corporation. All rights reserved.
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

function [fail, pass] = sof_test_perf(cfgfn)

if nargin < 1
	test.cfgfn = 'sof_test_perf_config.m';
else
	test.cfgfn = cfgfn;
end

%% Generic settings
test = test_defaults(test);

%% Plotting
test.plot_close_windows = 1;    % Workaround for visible windows if Octave hangs
test.plot_visible = 'off';      % Use off for batch tests and on for interactive
test.plot_passband_zoom = 1;    % Do a small plot box for only passband
test.plot_channels_combine = 1; % Plot channels into same figure

%% Cleanup
test.files_delete = 0;          % Set to 0 to inspect the audio data files

%% Paths to other test functions, plots, reports
addpath('std_utils');
addpath('test_utils');
mkdir_check('plots');
mkdir_check('reports');

%% Get playback test play and capture configuration
test = get_config(test);

%% Run tests
fail = 0;
pass = 0;
test.tf = [];

% Check that the configuration has been edited
if test.play_cfg.ssh && strcmp(test.play_cfg.user, 'user@host.domain')
	fprintf(1, 'Error: The configuration file %s\n', test.cfgfn);
	fprintf(1, 'must be edited from default to present an actual test\n');
	fprintf(1, 'case. Or edit into sof_audio_quality_test_top.m the\n');
	fprintf(1, 'new test configuration list to execute.\n\n');
	quit(1);
end

%% Gain
[tf(1), g_db] = g_test(test);

if tf(1) == 0
	%% Frequency response
	[tf(2), fr_db, fr_hz, fr_3db_hz] = fr_test(test);

	%% Total harmonic distortion plus noise
	[tf(3), thdnf_low, thdnf_high] = thdnf_test(test);
end

%% Print results
test.rfn = sprintf('reports/%s.txt', test.id);
test.rfh = fopen(test.rfn, 'w');

print_val(test, tf(1), g_db, 'dB', 'Gain', 1);

if tf(1) == 0
	print_val(test, tf(2), fr_db, '+/- dB', 'Frequency response');
	print_val(test, tf(2), fr_3db_hz / 1000, 'kHz', '-3 dB frequency');
	print_val(test, tf(3), thdnf_low, 'dB', 'THD+N -20 dBFS');
	print_val(test, tf(3), thdnf_high, 'dB', 'THD+N -1 dBFS');
else
	fprintf(1, '\n');
	fprintf(1, 'Warning: The gain test must pass before other tests are executed.\n');
end

fprintf('\n');
fclose(test.rfh);

%% Summary
fail = sum(tf);
pass = length(tf) - fail;

end

%%
%% Test execution with help of common functions
%%

function [fail, g_db] = g_test(test)

%% Reference: AES17 6.2.2 Gain
c = 20/48;
test.fu = c * test.fs;

%% Create input file
test = g_test_input(test);

%% Run test
test = remote_test_run(test);

%% Measure
test.ch = test.rec_cfg.ch;
test = g_test_measure(test);

%% Get output parameters
fail = test.fail;
g_db = test.g_db;
delete_check(test.files_delete, test.fn_in);
delete_check(test.files_delete, test.fn_out);

end

function [fail, fr_db, fr_hz, fr_3db_hz] = fr_test(test)

%% Reference: AES17 6.2.3 Frequency response
c1 = 20/48;
c2 = 23.9/48;
test.f_lo = 20;            % Measure start at 20 Hz
test.f_hi = c1 * test.fs;  % Measure end e.g. at 20 kHz
test.f_max = c2 * test.fs; % Sweep max e.g. at 23.9 kHz

%% Create input file
test = fr_test_input(test);

%% Run test
test = remote_test_run(test);

%% Measure
test.ch = test.rec_cfg.ch;
test = fr_test_measure(test);

fail = test.fail;
fr_db = test.rp;
fr_hz = [test.f_lo test.f_hi];
fr_3db_hz = test.fr3db_hz;
delete_check(test.files_delete, test.fn_in);
delete_check(test.files_delete, test.fn_out);

%% Print
test_result_print(test, 'Frequency response', 'FR');

end

function [fail, thdnf_low, thdnf_high] = thdnf_test(test)

%% Reference: AES17 6.3.2 THD+N ratio vs. frequency
c = 20/48;
test.f_start = 20;
test.f_end = c * test.fs;
test.fu = c * test.fs;

%% Create input file
test = thdnf_test_input(test);

%% Run test
test = remote_test_run(test);

%% Measure
test.ch = test.rec_cfg.ch;
test = thdnf_test_measure(test);
fail = test.fail;
thdnf_high = max(test.thdnf_high);
thdnf_low = max(test.thdnf_low);
delete_check(test.files_delete, test.fn_in);
delete_check(test.files_delete, test.fn_out);

%% Print
test_result_print(test, 'THD+N ratio vs. frequency', 'THDNF');

end

%%
%% Utilities
%%

function test = test_defaults(test)

test.fmt = 'wav';
test.fr_rp_max_db = [];
test.fr_mask_f = [];
test.fr_mask_lo = [];
test.fr_mask_hi = [];
test.plot_fr_axis = [];
test.plot_thdn_axis = [];

end

%% Get number of bits from format string
function bits = sample_format_bits(sft)

switch lower(sft)
	case 's16_le'
		bits = 16;
	case 's24_le'
		bits = 24;
	case 's24_3le'
		bits = 24;
	case 's24_4le'
		bits = 24;
	case 's32_le'
		bits = 32;
	otherwise
		error('Illegal sample format.');
end

end


%% Get configuration for test
function [test, play, rec] = get_config(test)
fh = fopen(test.cfgfn);
cfg = fscanf(fh, '%c');
fclose(fh);
eval(cfg);

fprintf('\nThe settings for remote playback are\n');
fprintf('Use ssh   : %d\n', play.ssh);
fprintf('User      : %s\n', play.user);
fprintf('Directory : %s\n', play.dir);
fprintf('Device    : %s\n', play.dev);
fprintf('format    : %s\n', play.sft);
fprintf('Channels  : %d\n', play.nch);
for i=1:length(play.ch)
	fprintf('Channel   : %d\n', play.ch(i));
end

fprintf('\nThe settings for capture are\n');
fprintf('Use ssh   : %d\n', rec.ssh);
fprintf('User      : %s\n', rec.user);
fprintf('Directory : %s\n', rec.dir);
fprintf('Device    : %s\n', rec.dev);
fprintf('format    : %s\n', rec.sft);
fprintf('Channels  : %d\n', rec.nch);
for i=1:length(play.ch)
	fprintf('Channel   : %d\n', rec.ch(i));
end
fprintf('\n');

rec.fmt = sprintf('-t wav -c %d -f %s -r %d', ...
	rec.nch, rec.sft, test.fs);

test.play_cfg = play;
test.rec_cfg = rec;
test.nch = test.play_cfg.nch;
test.ch = test.play_cfg.ch;
test.bits_in = sample_format_bits(test.play_cfg.sft);
test.bits_out = sample_format_bits(test.rec_cfg.sft);

end

%% Export test result plots
function test_result_print(t, testverbose, testbrief)

tstr = sprintf('%s %s', t.id, testverbose);
if nargin > 3
	title(t.ph, tstr);
else
	title(tstr);
end
pfn = sprintf('plots/%s_%s.png', t.id, testbrief);
print(pfn, '-dpng');

if t.plot_close_windows
	close all;
end

end

%% Export test result prints
function print_val(test, fail, val, unit, desc, header)

if nargin < 6
	header = 0;
end

if fail
	fstr = 'Fail';
else
	fstr = 'Pass';
end

if header
	fprintf('\n');
	str = sprintf('%7s, %18s', 'Verdict', 'Test case');
	for i = 1:length(val)
		str = sprintf('%s, %7s', str, sprintf('Ch%d', test.ch(i)));
	end
	str = sprintf('%s, %8s\n', str, 'Unit');
	fprintf(str);
	fprintf(test.rfh, str);
end

str = sprintf('%7s, %18s', fstr, desc);
for i = 1:length(val)
	str = sprintf('%s, %7.2f', str, val(i));
end
str = sprintf('%s, %8s\n', str, unit);

fprintf(str);
fprintf(test.rfh, str);
end
