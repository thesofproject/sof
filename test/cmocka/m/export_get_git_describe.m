% description = export_get_git_describe()
%
% Outputs
%   description - git describe output in successful run, otherwise Unknown

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright(c) 2022 Intel Corporation. All rights reserved.

function description = export_get_git_describe()

	git_cmd = 'git describe --dirty --long --always';
	[status, out] = system(git_cmd);
	description = strtrim(out);
	if status
		description = 'Unknown'
	end

end
