% pcan_wide_dynamic_func - Octave fixed-point model of WideDynamicFunction
%
% SPDX-License-Identifier: BSD-3-Clause
% Copyright(c) 2026 Intel Corporation. All rights reserved.

function y = pcan_wide_dynamic_func(x, gain_lut)
% Inputs:
%   x        - 32-bit unsigned input value (scalar or vector)
%   gain_lut - 125-entry int16 lookup table from pcan_gain_lookup
%
% Output:
%   y        - 16-bit signed interpolated gain factor (same shape as x)

	y = zeros(size(x), 'int16');

	for k = 1:numel(x)
		val = uint32(x(k));
		if val <= 2
			% Directly return lut[0], lut[1], or lut[2]
			y(k) = gain_lut(val + 1);
		else
			% Compute MSB (1 to 32)
			clz_val = 0;
			tmp = val;
			for b = 31:-1:0
				if bitand(tmp, bitshift(uint32(1), b)) ~= 0
					break;
				end
				clz_val = clz_val + 1;
			end
			interval = 32 - clz_val;

			% In C: lut pointer is offset by (4 * interval - 6)
			% 1-based index for lut[0]:
			idx = 4 * interval - 5;
			l0 = int32(gain_lut(idx));
			l1 = int32(gain_lut(idx + 1));
			l2 = int32(gain_lut(idx + 2));

			if interval < 11
				frac = bitand(bitshift(val, 11 - interval), uint32(1023)); % 0x3FF
			else
				frac = bitand(bitshift(val, -(interval - 11)), uint32(1023));
			end
			frac_i32 = int32(frac);

			res = bitshift(l2 * frac_i32, -5);
			res = res + bitshift(l1, 5);
			res = res * frac_i32;
			res = bitshift(res + 16384, -15); % (1 << 14) = 16384
			res = res + l0;

			y(k) = int16(res);
		end
	end
end
