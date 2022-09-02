% ref_auditory - Generate C header files for auditory library unit tests

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright(c) 2022 Intel Corporation. All rights reserved.

function ref_dct()

	path(path(), '../../../m');
	opt.describe = export_get_git_describe();

	%% Test 1,
	opt.test_n = 1;
	opt.num_in = 23;
	opt.num_out = 13;
	opt.type = 'DCT_II';
	opt.ortho = 'true';
	opt.bits = 16;
	get_ref_dct_matrix(opt);

	%% Test 2,
	opt.test_n = 2;
	opt.num_in = 42;
	opt.num_out = 42;
	opt.type = 'DCT_II';
	opt.ortho = 'true';
	opt.bits = 16;
	get_ref_dct_matrix(opt);

end

function get_ref_dct_matrix(opt)

	header_fn = sprintf('ref_dct_matrix_%d_test%d.h', opt.bits, opt.test_n);

	% TODO: type and ortho parameters are not supported
	dct_matrix = mfcc_get_dct_matrix(opt.num_out, opt.num_in, 2);

	switch opt.bits
		case 16
			qdm = export_quant_qxy(dct_matrix, 16, 15, false); % Q1.15
		case 32
			qdm = export_quant_qxy(dct_matrix, 32, 31, false); % Q1.31
		otherwise
			error('Illegal bits value');
	end

	define_prefix = sprintf('DCT_MATRIX_%d_TEST%d_', opt.bits, opt.test_n);
	vector_prefix = sprintf('dct_matrix_%d_test%d_', opt.bits, opt.test_n);

	fh = export_headerfile_open(header_fn);
	comment = sprintf('Created %s with script ref_matrix.m %s', ...
			  datestr(now, 0), opt.describe);
	export_comment(fh, comment);
	export_ndefine(fh, [define_prefix 'NUM_IN'], opt.num_in);
	export_ndefine(fh, [define_prefix 'NUM_OUT'], opt.num_out);
	export_sdefine(fh, [define_prefix 'TYPE'], opt.type);
	export_sdefine(fh, [define_prefix 'ORTHO'], opt.ortho);
	export_vector(fh, opt.bits, [vector_prefix 'matrix'], mat_to_vec(qdm));
	fclose(fh);
	fprintf(1, 'Exported %s.\n', header_fn);

end

function v = mat_to_vec(m)
	v = reshape(transpose(m), prod(size(m)), 1);
end
