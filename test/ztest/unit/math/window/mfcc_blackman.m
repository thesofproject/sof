% win = mfcc_blackman(length, a0)
%
% Input
%   length - length of window
%   a0 - optional, parameter a0 for equation
%
% Output
%   window coefficients
%
% https://en.wikipedia.org/wiki/Window_function#Blackman_window

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2022, Intel Corporation. All rights reserved.

function w = mfcc_blackman(win_length, a0)

	if nargin < 2
		a0 = 0.42; % Default in Kaldi compute-mfcc-feats
	end

	alpha = 1 - 2 * a0;
	a1 = 1/2;
	a2 = alpha / 2;

	n = (0:(win_length - 1))';
	a = 2 * pi / (win_length -1);
	w = a0 - a1 * cos(a * n ) + a2 * cos(2 * a * n / win_length);

end
