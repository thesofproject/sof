function crossover_paths(enable)

% crossover_paths(enable)
% enable - set to true to enable needed search path
%          set to false to disable the search paths
%

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2024, Intel Corporation. All rights reserved.

	common = '../../../../tools/tune/common';
	eq = '../../../../tools/tune/eq';
	# After #9187 merge use this:
	# eq = '../../eq_iir/tune';
	if enable
		addpath(common);
		addpath(eq);
	else
		rmpath(common);
		rmpath(eq);
	end
end
