% mel = mfcc_hz_to_mel(hz)
%
% Wikipedia https://en.wikipedia.org/wiki/Mel_scale

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2022, Intel Corporation. All rights reserved.

function mel = mfcc_hz_to_mel(hz)

% Magic value is 2595/log(10)
mel = 1126.9941805389 * log(1 + hz / 700);

end
