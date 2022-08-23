% export_vector(fh, bits, vname, data, msize)
%
% Inputs
%   fh - file handle
%   bits - number of bits
%   vname - variable name
%   data - vector of integer data
%   msize - optional, size of original matrix to guide formatting

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright(c) 2022 Intel Corporation. All rights reserved.

function export_vector(fh, bits, vname, data, msize)
	switch bits
		case 8
			vtype = 'int8_t';
			maxcol = 10;
		case 16
			vtype = 'int16_t';
			maxcol = 10;
		case 32
			vtype = 'int32_t';
			maxcol = 6;
		otherwise
			error('Unkown bits format')
	end

	if nargin == 5
		columns = msize(2);
		columns = min(columns, maxcol);
	else
		columns = maxcol;
	end

	rows = ceil(length(data) / columns);
	fprintf(fh, '\nstatic const %s %s[%d] = {\n', ...
		vtype, vname, length(data));
	i = 1;
	for j = 1:rows
		if bits > 16
			fprintf(fh, '\t%11d,', data(i));
		else
			fprintf(fh, '\t%6d,', data(i));
		end
		i = i + 1;
		for k = 2:columns
			if i <= length(data)
				if bits > 16
					fprintf(fh, ' %11d,', data(i));
				else
					fprintf(fh, ' %6d,', data(i));
				end
				i = i + 1;
			end
		end
		fprintf(fh, '\n');
	end
	fprintf(fh, '};\n');
end
