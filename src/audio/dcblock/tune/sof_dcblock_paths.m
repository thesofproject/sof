function sof_dcblock_paths(enable)

% dcblock_paths(enable)
% enable - set to true to enable needed search path
%          set to false to disable the search paths
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
