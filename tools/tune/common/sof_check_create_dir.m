function sof_check_create_dir(fn)

%% Check path of filename create directory if missing
%
% check_create_dir(fn)
%
% Inputs
%  fn - string with path and filename

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright(c) 2023 Intel Corporation. All rights reserved.

[filepath, ~, ~] = fileparts(fn);
if ~exist(filepath, "dir")
	mkdir(filepath);
end

end
