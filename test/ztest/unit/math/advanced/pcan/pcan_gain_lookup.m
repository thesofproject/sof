% pcan_gain_lookup - Compute PCAN continuous gain and generate LUT
%
% SPDX-License-Identifier: BSD-3-Clause
% Copyright(c) 2026 Intel Corporation. All rights reserved.

function [gain_lut, y_lut] = pcan_gain_lookup(config, input_bits)
% Inputs:
%   config.strength   - Exponent alpha (e.g., 0.95)
%   config.offset     - Additive constant delta (e.g., 80.0)
%   config.gain_bits  - Scale factor exponent (e.g., 21)
%   input_bits        - smoothing_bits - input_correction_bits
%
% Outputs:
%   gain_lut          - 125-entry int16 lookup table for WideDynamicFunction
%   y_lut             - Raw function values at octave evaluation points

	if nargin < 2
		input_bits = 10;
	end

	strength = config.strength;
	offset = config.offset;
	gain_bits = config.gain_bits;

	kWideDynamicFunctionBits = 32;
	kWideDynamicFunctionLUTSize = 4 * kWideDynamicFunctionBits - 3; % 125

	gain_lut = zeros(kWideDynamicFunctionLUTSize, 1, 'int16');

	% Evaluate point x in gain function
	function y = eval_gain(x_val)
		x_float = double(x_val) / double(bitshift(uint64(1), input_bits));
		g_float = double(bitshift(uint64(1), gain_bits)) * ((x_float + offset) ^ (-strength));
		if g_float > 32767
			y = int16(32767);
		else
			y = int16(round(g_float));
		end
	end

	gain_lut(1) = eval_gain(0);
	gain_lut(2) = eval_gain(1);

	% Intervals 2 through 32
	% In C: lut is offset by -6 so that interval 2 writes to lut[4*2]=lut[8] -> offset 2 in array
	for interval = 2:kWideDynamicFunctionBits
		x0 = bitshift(uint64(1), interval - 1);
		x1 = x0 + bitshift(x0, -1);
		if interval == kWideDynamicFunctionBits
			x2 = x0 + (x0 - 1);
		else
			x2 = 2 * x0;
		end

		y0 = int32(eval_gain(x0));
		y1 = int32(eval_gain(x1));
		y2 = int32(eval_gain(x2));

		diff1 = y1 - y0;
		diff2 = y2 - y0;
		a1 = 4 * diff1 - diff2;
		a2 = diff2 - a1;

		% Map to 1-based index in gain_lut:
		% In C: index is 4 * interval - 6 (0-based) -> +1 for 1-based
		idx = 4 * interval - 5;

		gain_lut(idx) = int16(y0);
		gain_lut(idx + 1) = int16(a1);
		gain_lut(idx + 2) = int16(a2);
		if interval < kWideDynamicFunctionBits
			gain_lut(idx + 3) = int16(0);
		end
	end

	y_lut = gain_lut;
end
