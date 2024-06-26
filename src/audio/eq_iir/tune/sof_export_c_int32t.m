% Export to C header filer int32_t data
%
% Usage:
% sof_export_c_int32t(fn, vn, ln, x)
% Inputs:
%   fn - filename for header file
%   vn - variable name
%   ln - name for defined LENGTH
%   x - matrix of data

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2021, Intel Corporation. All rights reserved.

function sof_export_c_int32t(fn, vn, ln, x)

% Write blob
blob32 = interleave(x);
count = length(blob32);
fh = fopen(fn, 'w');
if fh < 0
	fprintf(1, 'Error: Could not open file: %s\n', fn);
	error("Failed.");
end

numbers_in_line = 6;
full_lines = floor(count/numbers_in_line);
numbers_remain = count - numbers_in_line * full_lines;

year = datestr(now, 'yyyy');
fprintf(fh, '/* SPDX-License-Identifier: BSD-3-Clause\n');
fprintf(fh, ' *\n');
fprintf(fh, ' * Copyright(c) %s Intel Corporation. All rights reserved.\n', year);
fprintf(fh, ' */\n\n');
fprintf(fh, '#define %s %d\n\n', ln, count);

n = 1;
fprintf(fh, 'int32_t %s[%s] = {\n', vn, ln);
for i = 1:full_lines
	fprintf(fh, '\t');
	for j = 1:numbers_in_line
		fprintf(fh, '%dLL', blob32(n));
		if n < count
			fprintf(fh, ',');
		end
		if j < numbers_in_line
			fprintf(fh, ' ');
		else
			fprintf(fh, '\n');
		end
		n = n + 1;
	end
end

if numbers_remain
	fprintf(fh, '\t');
	for j = 1:numbers_remain
		fprintf(fh, '%dLL', blob32(n));
		if n < count
			fprintf(fh, ',');
		end
		if j < numbers_remain
			fprintf(fh, ' ');
		else
			fprintf(fh, '\n');
		end
		n = n + 1;
	end
end

fprintf(fh, '};\n');
fclose(fh);
end

function y = interleave(x)

sx = size(x);
samples = sx(1);
channels = sx(2);
if sx(2) > 1
	y = zeros(samples * channels, 1);
	for i = 1:channels
		y(i:channels:end) = x(:, i);
	end
else
	y = x;
end
end
