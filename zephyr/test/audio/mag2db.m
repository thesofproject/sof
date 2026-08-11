% mag2db - Convert magnitude (linear scale) to dB
% Provides mag2db when Octave 'control' package is not installed.
%
% SPDX-License-Identifier: BSD-3-Clause
% Copyright(c) 2026 Intel Corporation.

function y = mag2db(x)
	y = 20 * log10(x);
end
