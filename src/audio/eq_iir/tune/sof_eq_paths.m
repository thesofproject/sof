function sof_eq_paths(enable)

% sof_eq_paths(enable)
% enable - set to 1 to enable needed search path
%          set to 0 to disable the search paths
%

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2024, Intel Corporation. All rights reserved.

	common = '../../../../tools/tune/common';
	if enable
		addpath(common);
	else
		rmpath(common);
	end
end
