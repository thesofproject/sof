function sof_bf_paths(enable)

% sof_bf_paths(enable)
% enable - set to true to enable needed search path
%          set to false to disable the search paths
%

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2024, Intel Corporation.

	sof_tools = '../../../../tools';
	sof_modules = '../..';
	common = fullfile(sof_tools, 'tune/common');
	eq = fullfile(sof_modules, 'eq_iir/tune');
	test_utils = fullfile(sof_tools, 'test/audio/test_utils');
	std_utils = fullfile(sof_tools, 'test/audio/std_utils');
	if enable
		addpath(common);
		addpath(eq);
		addpath(test_utils);
		addpath(std_utils);
	else
		rmpath(common);
		rmpath(eq);
		rmpath(test_utils);
		rmpath(std_utils);
	end
end
