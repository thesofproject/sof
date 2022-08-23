% [ival, qval] = export_quant_qxy(val, bits, fracbits, saturate)
%
% Inputs
%   val - values to quantize
%   bits - number of bits
%   fracbits - the number of bits y in Qx.y format
%   saturate - true or false, when false overflow is an error.
%              false if omitted.
%
% Outputs
%   ival - quantized value as integer
%   qval - quantized value as float, same scale as val

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright(c) 2022 Intel Corporation. All rights reserved.

function [ival, qval] = export_quant_qxy(val, bits, fracbits, saturate)

	if nargin < 4
		saturate = false;
	end
	intmax = 2^(bits - 1) - 1;
	intmin = -intmax - 1;
	scale = 2^fracbits;
	yf = round(val * scale);
	if saturate == true
		yf(yf > intmax) = intmax;
		yf(yf < intmin) = intmin;
	end

	min_y = min(min(yf));
	max_y = max(max(yf));
	if max_y > intmax || min_y < intmin
		fprintf(1, 'max = %d (%d), min = %d (%d)\n', ...
			max_y, intmax, min_y, intmin);
		error('Overflow, use other Q format');
	end

	switch bits
		case 8
			ival = int8(yf);
		case 16
			ival = int16(yf);
		case 32
			ival = int32(yf);
		otherwise
			error('Unknown bits');
	end

	qval = double(ival) / scale;
end
