function multiband_drc_paths(enable)

% multiband_drc_paths(enable)
% enable - set to true to enable needed search path
%          set to false to disable the search paths
%

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2024, Intel Corporation. All rights reserved.

	common = '../../../../tools/tune/common';
	crossover = '../../../../tools/tune/crossover';
	drc = '../../../../tools/tune/drc';
	eq = '../../../../tools/tune/eq';
	# After #9215 merge use this:
	# crossover = '../../crossover/tune';
	# After #9188 merge use this:
	# drc = '../../drc/tune';
	# After #9187 merge use this:
	# eq = '../../eq_iir/tune';
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
