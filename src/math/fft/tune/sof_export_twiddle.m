% export_twiddle(bits, fn, fft_size_max)
%
% Input
%   bits - Number of bits for data, 16 or 32
%   fn - File name, defaults to twiddle.h
%   fft_size_max - Number of twiddle factors, defaults to 1024 if omitted

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2022, Intel Corporation. All rights reserved.

function export_twiddle(bits, fn, fft_size_max)

if nargin < 2
	fn = 'twiddle.h';
end

if nargin < 3
	fft_size_max = 1024;
end

switch bits
	case 16,
		qx = 1;
		qy = 15;
	case 32,
		qx = 1;
		qy = 31;
	otherwise,
		error('Illegal bits count');
end

[~, hname, ~] =  fileparts(fn);
hcaps = upper(hname);

i = 0:(fft_size_max  - 1);
twiddle_real = cos(i * 2 * pi / fft_size_max);
twiddle_imag = -sin(i * 2 * pi / fft_size_max);

year = datestr(now(), 'yyyy');
fh = fopen(fn, 'w');
fprintf(fh, '/* SPDX-License-Identifier: BSD-3-Clause\n');
fprintf(fh, ' *\n');
fprintf(fh, ' * Copyright(c) %s Intel Corporation. All rights reserved.\n', year);
fprintf(fh, ' *\n');
fprintf(fh, ' */\n\n');
fprintf(fh, '/* Twiddle factors in Q%d.%d format */\n\n', qx, qy);
fprintf(fh, '#ifndef __INCLUDE_%s_H__\n', hcaps);
fprintf(fh, '#define __INCLUDE_%s_H__\n\n', hcaps);
fprintf(fh, '#include <stdint.h>\n\n');
fprintf(fh, '#define FFT_SIZE_MAX	%d\n\n', fft_size_max);
fprintf(fh, '/* in Q1.%d, generated from cos(i * 2 * pi / FFT_SIZE_MAX) */\n', qy);
c_export_int(fh, 'twiddle_real', 'FFT_SIZE_MAX', twiddle_real, qx, qy);

fprintf(fh, '/* in Q1.%d, generated from sin(i * 2 * pi / FFT_SIZE_MAX) */\n', qy);
c_export_int(fh, 'twiddle_imag', 'FFT_SIZE_MAX', twiddle_imag, qx, qy);

fprintf(fh, '#endif\n');
fclose(fh);

fprintf(1, 'Exported successfully file %s\n', fn);

end

function c_export_int(fh, vname, vsize, data, qx, qy, vn)

bits = qx + qy;
switch bits
	case 16,
		vtype = 'int16_t';
	case 32,
		vtype = 'int32_t';
end

maxint = 2^(qx + qy - 1) - 1;
minint = -2^(qx + qy - 1) + 1; % Avoid "this decimal constant is unsigned only in ISO C90"
scale = 2^qy;
quant = round(data * scale);
quant = min(quant, maxint);
quant = max(quant, minint);

fprintf(fh, 'const %s %s_%d[%s] = {\n', vtype, vname, bits, vsize);
for i = 1:length(data)
	fprintf(fh, '\t%d,\n', quant(i));
end

fprintf(fh, '};\n\n');

end
