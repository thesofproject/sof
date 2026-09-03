% ref_dft3 - Generate C header files for DFT3 function unit tests

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright(c) 2025 Intel Corporation. All rights reserved.

function ref_dft3()

	path(path(), '../../../m');
	opt.describe = export_get_git_describe();

	N = 3;
	num_tests = 100;
	scale_q31 = 2^31;
	opt.bits = 32;
	min_int32 = int32(-2^31);
	max_int32 = int32(2^31 - 1);

	% Random values
	input_data_real_q31 = int32((2 * rand(N, num_tests) - 1) * scale_q31);
	input_data_imag_q31 = int32((2 * rand(N, num_tests) - 1) * scale_q31);

	% Apply max and min values to first two tests
	input_data_real_q31(:,1) = [max_int32 max_int32 max_int32];
	input_data_imag_q31(:,1) = [max_int32 max_int32 max_int32];
	input_data_real_q31(:,2) = [min_int32 min_int32 min_int32];
	input_data_imag_q31(:,2) = [min_int32 min_int32 min_int32];

	% Convert to float for reference DFT
	input_data_real_f = double(input_data_real_q31) / scale_q31;
	input_data_imag_f = double(input_data_imag_q31) / scale_q31;
	input_data_f = complex(input_data_real_f, input_data_imag_f);

	ref_data_f = zeros(N, num_tests);
	for i = 1:num_tests
		ref_data_f(:,i) = fft(1/N * input_data_f(:,i));
	end

	input_data_vec_f = reshape(input_data_f, N * num_tests, 1);
	input_data_real_q31 = int32(real(input_data_vec_f) * scale_q31);
	input_data_imag_q31 = int32(imag(input_data_vec_f) * scale_q31);

	ref_data_vec_f = reshape(ref_data_f, N * num_tests, 1);
	ref_data_real_q31 = int32(real(ref_data_vec_f) * scale_q31);
	ref_data_imag_q31 = int32(imag(ref_data_vec_f) * scale_q31);

	header_fn = sprintf('ref_dft3_32.h');
	fh = export_headerfile_open(header_fn);
	comment = sprintf('Created %s with script ref_dft3.m %s', ...
			  datestr(now, 0), opt.describe);
	export_comment(fh, comment);
	export_ndefine(fh, 'REF_SOFM_DFT3_NUM_TESTS', num_tests);
	export_vector(fh, opt.bits, 'input_data_real_q31', input_data_real_q31);
	export_vector(fh, opt.bits, 'input_data_imag_q31', input_data_imag_q31);
	export_vector(fh, opt.bits, 'ref_data_real_q31', ref_data_real_q31);
	export_vector(fh, opt.bits, 'ref_data_imag_q31', ref_data_imag_q31);
	fclose(fh);
	fprintf(1, 'Exported %s.\n', header_fn);
end
