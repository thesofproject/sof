% hz = mfcc_mel_to_hz(mel)
%
% Wikipedia https://en.wikipedia.org/wiki/Mel_scale

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2022, Intel Corporation. All rights reserved.

function hz = mfcc_mel_to_hz(mel)

hz = 700 * (exp(mel / 1126.9941805389) - 1);

end
