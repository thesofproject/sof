function sof_multiband_drc_paths(enable)

% sof_multiband_drc_paths(enable)
% enable - set to true to enable needed search path
%          set to false to disable the search paths
%

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2024, Intel Corporation. All rights reserved.

	common = '../../../../tools/tune/common';
	crossover = '../../crossover/tune';
	drc = '../../drc/tune';
	eq = '../../eq_iir/tune';
	if enable
		addpath(common);
		addpath(crossover);
		addpath(drc);
		addpath(eq);
	else
		rmpath(common);
		rmpath(crossover);
		rmpath(drc);
		rmpath(eq);
	end
end
