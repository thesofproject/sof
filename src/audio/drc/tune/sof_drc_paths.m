function sof_drc_paths(enable)

% sof_drc_paths(enable)
% enable - set to 1 to enable needed search path
%          set to 0 to disable the search paths
%

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2024, Intel Corporation. All rights reserved.

	common = '../../../../tools/tune/common';
	eq = '../../eq_iir/tune';
	if enable
		addpath(common);
		addpath(eq);
	else
		rmpath(common);
		rmpath(eq);
	end
end
