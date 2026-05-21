% db2mag - Convert dB to magnitude (linear scale)
% Provides db2mag when Octave 'control' package is not installed.
%
% SPDX-License-Identifier: BSD-3-Clause
% Copyright(c) 2026 Intel Corporation.

function y = db2mag(x)
	y = 10 .^ (x / 20);
end
