function export_c_eq_uint32t(fn, blob8, vn, justeq)

% export_c_eq_uint32t(fn, blob8, vn)
%
% Export 8-bit blob data as C int32_t vector in hexadecimal
%
% fn - filename for export
% blob8 - blob data from EQ design
% vn - variable name
% justeq - export just the EQ wihtout ABI headers for direct use in FW

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2021, Intel Corporation. All rights reserved.

	% Write blob
	fh = fopen(fn, 'w');
	year = datestr(now, 'yyyy');
	fprintf(fh, '/* SPDX-License-Identifier: BSD-3-Clause\n');
	fprintf(fh, ' *\n');
	fprintf(fh, ' * Copyright(c) %s Intel Corporation. All rights reserved.\n', year);
	fprintf(fh, ' */\n');
	fprintf(fh, '\n');

	% Pad blob length to multiple of four bytes
	n_orig = length(blob8);
	n_new = ceil(n_orig/4);
	blob8_new = zeros(1, n_new*4);
	blob8_new(1:n_orig) = blob8;

	%% Convert to 32 bit
	blob32 = zeros(1, n_new, 'uint32');
	k = 2.^[0 8 16 24];
	for i=1:n_new
		j = (i-1)*4;
		blob32(i) = blob8_new(j+1)*k(1) + blob8_new(j+2)*k(2) ...
			    +  blob8_new(j+3)*k(3) + blob8_new(j+4)*k(4);
	end

	% Omit all headers if requested
	if justeq
		blob32 = blob32(18:end);
		n_new = length(blob32);
	end

	numbers_in_line = 4;
	full_lines = floor(n_new/numbers_in_line);
	numbers_remain = n_new - numbers_in_line * full_lines;

	n = 1;
	fprintf(fh, 'uint32_t %s[%d] = {\n', vn, n_new);
	for i = 1:full_lines
		fprintf(fh, '\t');
		for j = 1:numbers_in_line
			fprintf(fh, '0x%08x', blob32(n));
			if n < n_new
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
			fprintf(fh, '0x%08x', blob32(n));
			if n < n_new
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
