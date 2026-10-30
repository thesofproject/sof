% pcan_shrink - Octave fixed-point model of PcanShrink piecewise compression
%
% SPDX-License-Identifier: BSD-3-Clause
% Copyright(c) 2026 Intel Corporation. All rights reserved.

function y = pcan_shrink(x, snr_bits, output_bits)
% Inputs:
%   x           - 32-bit unsigned SNR input value (scalar or vector)
%   snr_bits    - Number of fractional bits in SNR (default 12)
%   output_bits - Number of fractional bits in output (default 6)
%
% Output:
%   y           - 32-bit unsigned compressed output value

	if nargin < 2
		snr_bits = 12;
	end
	if nargin < 3
		output_bits = 6;
	end

	threshold = bitshift(uint64(2), snr_bits); % 2 << 12 = 8192
	quadratic_shift = -(2 + 2 * snr_bits - output_bits); % -20
	linear_shift = -(snr_bits - output_bits); % -6
	linear_offset = uint64(bitshift(1, output_bits)); % 64

	y = zeros(size(x), 'uint32');

	for k = 1:numel(x)
		val = uint64(x(k));
		if val < threshold
			% Quadratic compression: x^2 / 4
			prod_val = val * val;
			res = bitshift(prod_val, quadratic_shift);
		else
			% Linear compression: x - 1
			res = bitshift(val, linear_shift) - linear_offset;
		end
		y(k) = uint32(res);
	end
end
