%% Export full set of conversions for src-lite

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright(c) 2022 Intel Corporation. All rights reserved.

fs1 = [ 32 44.1 48 ] * 1e3;

fs2 = [ 16 48 ] * 1e3;

fs_matrix = [
     1     1
     1     1
     1     1

];

cfg.ctype = 'int32';
cfg.profile = 'lite';
cfg.quality = 1.0;
cfg.thdn = -92;
cfg.speed = 0;
cfg.gain = 0; % Make gain 0 dB

sof_src_generate(fs1, fs2, fs_matrix, cfg);
