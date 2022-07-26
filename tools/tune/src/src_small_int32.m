% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2020, Intel Corporation. All rights reserved.

fs1 = [8 16 24 32 44.1 48 50] * 1e3;

fs2 = [8 16 24 32 44.1 48 50] * 1e3;

%             In
%             8 1 2 3 4 4 5
%               6 4 2 4 8 0        Out
fs_matrix = [ 0 1 1 1 0 1 0 ; ... %  8
	      1 0 1 1 0 1 0 ; ... % 16
	      1 1 0 1 0 1 0 ; ... % 24
	      1 1 1 0 0 1 0 ; ... % 32
	      0 0 0 0 0 1 0 ; ... % 44
	      1 1 1 1 1 0 1 ; ... % 48
	      0 0 0 0 0 1 0 ; ... % 50
	    ];

cfg.ctype = 'int32';
cfg.profile = 'small';
cfg.quality = 1.0;
cfg.speed = 0;
cfg.gain = -1; % Make gain -1 dB

src_generate(fs1, fs2, fs_matrix, cfg);
