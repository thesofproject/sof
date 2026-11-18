% win = mfcc_povey(length)
%
% Input
%   length
%
% Output
%   window coefficients
%
% https://kaldi-asr.org/doc/feature-window_8h_source.html
%
% "povey is a window I made to be similar to Hamming but to go to
%  zero at the edges"

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2022, Intel Corporation. All rights reserved.

function w = mfcc_povey(win_length)

	n = (0:(win_length - 1))';
	a = 2 * pi / (win_length - 1);
	w = (0.5 - 0.5 * cos(a * n)) .^ 0.85;

end
