% ref_matrix - Generate C header files for matrix library unit tests

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright(c) 2022 Intel Corporation. All rights reserved.

function ref_matrix()
	opt.test_n = 1;
	opt.input = 'random';
	opt.a_rows = 3;
	opt.a_columns = 4;
	opt.b_rows = 4;
	opt.b_columns = 5;
	opt.a_qxy_x = 1;
	opt.a_qxy_y = 15;
	opt.b_qxy_x = 1;
	opt.b_qxy_y = 15;
	opt.c_qxy_x = 2;
	opt.c_qxy_y = 14;
	opt.elementwise = 0;
	get_ref_mult(opt);

	opt.test_n = 2;
	opt.input = 'random';
	opt.a_rows = 6;
	opt.a_columns = 7;
	opt.b_rows = 7;
	opt.b_columns = 8;
	opt.a_qxy_x = 7;
	opt.a_qxy_y = 0;
	opt.b_qxy_x = 7;
	opt.b_qxy_y = 0;
	opt.c_qxy_x = 16;
	opt.c_qxy_y = 0;
	opt.elementwise = 0;
	get_ref_mult(opt);

	opt.test_n = 3;
	opt.input = 'random';
	opt.a_rows = 5;
	opt.a_columns = 6;
	opt.b_rows = 5;
	opt.b_columns = 6;
	opt.a_qxy_x = 1;
	opt.a_qxy_y = 15;
	opt.b_qxy_x = 1;
	opt.b_qxy_y = 15;
	opt.c_qxy_x = 1;
	opt.c_qxy_y = 15;
	opt.elementwise = 1;
	get_ref_mult(opt);

	opt.test_n = 4;
	opt.input = 'random';
	opt.a_rows = 6;
	opt.a_columns = 1;
	opt.b_rows = 1;
	opt.b_columns = 8;
	opt.a_qxy_x = 1;
	opt.a_qxy_y = 15;
	opt.b_qxy_x = 1;
	opt.b_qxy_y = 15;
	opt.c_qxy_x = 1;
	opt.c_qxy_y = 15;
	opt.elementwise = 0;
	get_ref_mult(opt);
end

function get_ref_mult(opt)
	[a_bits, a_scale] = get_qxy_scale(opt.a_qxy_x, opt.a_qxy_y);
	[b_bits, b_scale] = get_qxy_scale(opt.b_qxy_x, opt.b_qxy_y);
	[c_bits, c_scale] = get_qxy_scale(opt.c_qxy_x, opt.c_qxy_y);
	bits = max([a_bits b_bits c_bits]);

	% Data to process
	switch lower(opt.input)
		case 'ones'
			a = ones(opt.a_rows, opt.a_columns);
			b = ones(opt.b_rows, opt.b_columns);
		case 'random'
			a = (rand(opt.a_rows, opt.a_columns) * 2 - 1) * a_scale;
			b = (rand(opt.b_rows, opt.b_columns) * 2 - 1) * b_scale;
		otherwise
			error('Illegal input');
	end

	[iqa, fqa] = quant_qxy(a, bits, opt.a_qxy_y, true);
	[iqb, fqb] = quant_qxy(b, bits, opt.b_qxy_y, true);
	if opt.elementwise
		c = fqa .* fqb;
	else
		c = fqa * fqb;
	end

	iqc = quant_qxy(c, bits, opt.c_qxy_y, false);
	c_size = size(c);

	header_fn = sprintf('ref_matrix_mult_%d_test%d.h', bits, opt.test_n);
	define_prefix = sprintf('MATRIX_MULT_%d_TEST%d_', bits, opt.test_n);

	fh = export_headerfile_open(header_fn);
	comment = sprintf('Created %s with script ref_matrix.m %s', ...
			  datestr(now, 0), get_git_describe());
	export_comment(fh, comment);
	export_ndefine(fh, [define_prefix 'ELEMENTWISE'], opt.elementwise);
	export_ndefine(fh, [define_prefix 'A_ROWS'], opt.a_rows);
	export_ndefine(fh, [define_prefix 'A_COLUMNS'], opt.a_columns);
	export_ndefine(fh, [define_prefix 'B_ROWS'], opt.b_rows);
	export_ndefine(fh, [define_prefix 'B_COLUMNS'], opt.b_columns);
	export_ndefine(fh, [define_prefix 'C_ROWS'], c_size(1));
	export_ndefine(fh, [define_prefix 'C_COLUMNS'], c_size(2));
	export_ndefine(fh, [define_prefix 'A_QXY_Y'], opt.a_qxy_y);
	export_ndefine(fh, [define_prefix 'B_QXY_Y'], opt.b_qxy_y);
	export_ndefine(fh, [define_prefix 'C_QXY_Y'], opt.c_qxy_y);

	vector_name_a = sprintf('matrix_mult_%d_test%d_a', bits, opt.test_n);
	vector_name_b = sprintf('matrix_mult_%d_test%d_b', bits, opt.test_n);
	vector_name_c = sprintf('matrix_mult_%d_test%d_c', bits, opt.test_n);
	export_vector(fh, bits, vector_name_a, mat_to_vec(iqa), size(iqa));
	export_vector(fh, bits, vector_name_b, mat_to_vec(iqb), size(iqb));
	export_vector(fh, bits, vector_name_c, mat_to_vec(iqc), size(iqc));
  fclose(fh);
	fprintf(1, 'Exported file %s\n', header_fn);
end

function [bits, scale] = get_qxy_scale(qxy_x, qxy_y)
	bits = qxy_x + qxy_y;
	scale = 2^(bits - 1) / 2^qxy_y;
end

function v = mat_to_vec(m)
	v = reshape(transpose(m), prod(size(m)), 1);
end

function [yq,qval] = quant_qxy(val, bits, y, saturate)
	intmax = 2^(bits - 1) - 1;
	intmin = -intmax - 1;
	scale = 2^y;
	yf = round(val * scale);
	if saturate == true
		yf(yf > intmax) = intmax;
		yf(yf < intmin) = intmin;
	end

	min_y = min(min(yf));
	max_y = max(max(yf));
	if max_y > intmax || min_y < intmin
		fprintf(1, 'max_y = %d (%d), min_y = %d (%d)\n', ...
			max_y, intmax, min_y, intmin);
		error('Overflow, use other Q format');
	end

	switch bits
		case 8
			yq = int8(yf);
		case 16
			yq = int16(yf);
		case 32
			yq = int32(yf);
		otherwise
			error('Unknown bits');
	end

	qval = double(yq) / scale;
end

function fh = export_headerfile_open(headerfn)
	fh = fopen(headerfn, 'w');
	fprintf(fh, '/* SPDX-License-Identifier: BSD-3-Clause\n');
	fprintf(fh, ' *\n');
	fprintf(fh, ' * Copyright(c) %s Intel Corporation. All rights reserved.\n', ...
		datestr(now, 'yyyy'));
	fprintf(fh, ' */\n\n');
end

function export_ndefine(fh, dn, dval)
	fprintf(fh, '#define %s  %d\n', dn, dval);
end

function export_comment(fh, text)
	fprintf(fh, '/* %s */\n\n', text);
end

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
	fprintf(fh, '\nstatic const %s %s[%d] = {\n', vtype, vname, length(data));
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

function description = get_git_describe()
	git_cmd = 'git describe --dirty --long --always';
	[status, out] = system(git_cmd);
	description = strtrim(out);
	if status
		description = 'Unknown'
	end
end
