%% Export full set of conversions for IPC4

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright(c) 2022 Intel Corporation. All rights reserved.

fs1 = [ 8 11.025 12 16 18.9 22.05 24 32 37.8 44.1 48 64 88.2 96 176.4 192 ] * 1e3;

fs2 = [8 16 24 32 44.1 48 88.2 96 176.4 192] * 1e3;

fs_matrix = [
     1     1     1     1     1     1     1     1     1     1
     1     1     1     1     1     1     1     1     1     0
     1     1     1     1     1     1     1     1     1     1
     1     1     1     1     1     1     1     1     1     1
     0     0     1     0     1     1     0     0     0     0
     1     1     1     1     1     1     1     1     1     1
     1     1     1     1     1     1     1     1     1     1
     1     1     1     1     1     1     1     1     1     1
     0     0     0     0     1     1     1     1     0     0
     1     1     1     1     1     1     1     1     1     1
     1     1     1     1     1     1     1     1     1     1
     1     1     1     1     1     1     1     1     1     1
     1     1     1     1     1     1     1     1     1     1
     1     1     1     1     1     1     1     1     1     1
     1     1     1     1     1     1     1     1     1     1
     1     1     1     1     1     1     1     1     1     1
];

cfg.ctype = 'int32';
cfg.profile = 'ipc4';
cfg.quality = 1.0;
cfg.speed = 0;
cfg.gain = 0; % Make gain 0 dB

src_generate(fs1, fs2, fs_matrix, cfg);
