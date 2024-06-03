function src_paths(enable)

% src_paths(enable)
% enable - set to 1 to enable needed search path
%          set to 0 to disable the search paths
%

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2024, Intel Corporation. All rights reserved.

audio_test = '../../../../tools/test/audio/';

if enable
	addpath(fullfile(audio_test, 'std_utils'));
	addpath(fullfile(audio_test, 'test_utils'));
else
	rmpath(fullfile(audio_test, 'std_utils'));
	rmpath(fullfile(audio_test, 'test_utils'));
end

end
