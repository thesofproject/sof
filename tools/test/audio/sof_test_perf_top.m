function sof_test_perf_top

%%
% audio_test_perf_top - Wrapper function for call from shell
%
% audio_test_perf_top()
%
% Inputs
%   none
%
% Outputs
%   shell exit code
%

% SPDX-License-Identifier: BSD-3-Clause
% Copyright(c) 2019 Intel Corporation. All rights reserved.
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

% Edit to next lines the configs list the actual tests to
% execute. The syntax used is called a cell array. The
% script will execute test with each configuration in the
% array. E.g.
% configs = {'up2_s16_play_cfg.m', 't100_s16_play_cfg.m'};
configs = {'sof_test_perf_config.m'};

%% Run tests
n_fail_tot = 0;
n_pass_tot = 0;
n = length(configs);
for i = 1:n
	cfg = char(configs(i));
	[n_fail, n_pass] = sof_test_perf(cfg);
	n_fail_tot = n_fail_tot + n_fail;
	n_pass_tot = n_pass_tot + n_pass;

	fprintf('Completed: %s\n', cfg);
	fprintf('Passed   : %d\n', n_pass);
	fprintf('Failed   : %d\n\n', n_fail);
end

if n > 1
	fprintf('Completed all.\n');
	fprintf('Passed   : %d\n', n_pass_tot);
	fprintf('Failed   : %d\n', n_fail_tot);
end

if n_fail_tot > 0 || n_pass_tot < 1
	fprintf('Error: Audio quality test failed.\n');
	quit(1)
end

fprintf('Success: Audio quality test passed.\n');
