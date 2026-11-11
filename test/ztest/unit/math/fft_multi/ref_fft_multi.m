% ref_sofm_dft3 - Generate C header files for DFT3 function unit tests

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright(c) 2025 Intel Corporation. All rights reserved.

function ref_fft_multi()

    rand('twister', 0); % Set seed to produce same test vectors every time
    path(path(), '../../../m');
    opt.describe = export_get_git_describe();
    opt.bits = 32;
    opt.fs = 48e3;
    opt.ifft = 0;
    opt.sine = 1;
    opt.rand = 0;
    opt.dc = 0;
    opt.num_tests = 1;

    N = 96;
    make_fft_multi_test_vectors(opt, N);

    N = 512;
    make_fft_multi_test_vectors(opt, N);

    N = 768;
    make_fft_multi_test_vectors(opt, N);

    N = 1024;
    make_fft_multi_test_vectors(opt, N);

    N = 1536;
    make_fft_multi_test_vectors(opt, N);

    N = 3072;
    make_fft_multi_test_vectors(opt, N);

    opt.ifft = 1;
    opt.dc = 0;
    opt.sine = 1;
    opt.rand = 0;

    N = 24;
    make_fft_multi_test_vectors(opt, N);

    N = 256;
    make_fft_multi_test_vectors(opt, N);

    N = 1024;
    make_fft_multi_test_vectors(opt, N);

    N = 1536;
    make_fft_multi_test_vectors(opt, N);

    N = 3072;
    make_fft_multi_test_vectors(opt, N);

end

function make_fft_multi_test_vectors(opt, N)

    scale_q = 2^(opt.bits - 1);
    min_int = int32(-scale_q);
    max_int = int32(scale_q - 1);
    n = 1;

    input_data_real_q = int32(zeros(N, opt.num_tests));
    input_data_imag_q = int32(zeros(N, opt.num_tests));

    if opt.dc
	input_data_real_q(:,n) = int32(ones(N, 1) * scale_q / N);
        n = n + 1;
    end
    if opt.rand
	input_data_real_q(:,n) = int32(2 * (rand(N, 1) - 1) * scale_q * 0.1);
	input_data_imag_q(:,n) = int32(2 * (rand(N, 1) - 1) * scale_q * 0.1);
        n = n + 1;
    end
    if opt.sine
	ft = 997;
	t = (0:(N - 1))'/opt.fs;
	x = 10^(-1 / 20) * sin(2 * pi * ft * t) .* kaiser(N, 20);
	dither = scale_q / 2^19 * (rand(N, 1) + rand(N, 1) - 1);
	input_data_real_q(:,n) = int32(x * scale_q + dither);
        if opt.ifft
            tmp_fft = fft(double(input_data_real_q(:,n)) / scale_q) / N;
            input_data_real_q(:,n) = int32(real(tmp_fft) * scale_q);
            input_data_imag_q(:,n) = int32(imag(tmp_fft) * scale_q);
        end
        n = n + 1;
    end

    if opt.ifft
        N_half = N/2 + 1;
        for i = 1:opt.num_tests
            input_data_real_q(N_half + 1:end) = input_data_real_q(N_half - 1:-1:2);
            input_data_imag_q(N_half + 1:end) = -input_data_imag_q(N_half - 1:-1:2);
        end
    end

    input_data_real_f = double(input_data_real_q) / scale_q;
    input_data_imag_f = double(input_data_imag_q) / scale_q;
    input_data_f = complex(input_data_real_f, input_data_imag_f);

    ref_data_f = zeros(N, opt.num_tests);
    for i = 1:opt.num_tests
        if opt.ifft
	    ref_data_f(:,i) = ifft(input_data_f(:,i)) * N;
            test_type = 'ifft';
        else
	    ref_data_f(:,i) = fft(input_data_f(:,i)) / N;
            test_type = 'fft';
        end
    end

    input_data_vec_f = reshape(input_data_f, N * opt.num_tests, 1);
    input_data_real_q = int32(real(input_data_vec_f) * scale_q);
    input_data_imag_q = int32(imag(input_data_vec_f) * scale_q);

    ref_data_vec_f = reshape(ref_data_f, N * opt.num_tests, 1);
    ref_data_real_q = int32(real(ref_data_vec_f) * scale_q);
    ref_data_imag_q = int32(imag(ref_data_vec_f) * scale_q);

    header_fn = sprintf('ref_%s_multi_%d_%d.h', test_type, N, opt.bits);
    fh = export_headerfile_open(header_fn);
    comment = sprintf('Created %s with script ref_fft_multi.m %s', ...
		      datestr(now, 0), opt.describe);
    export_comment(fh, comment);
    dstr = sprintf('REF_SOFM_%s_MULTI_%d_NUM_TESTS', upper(test_type), N);
    export_ndefine(fh, dstr, opt.num_tests);
    qbits = opt.bits-1;
    vstr = sprintf('%s_in_real_%d_q%d', test_type, N, qbits); export_vector(fh, opt.bits, vstr, input_data_real_q);
    vstr = sprintf('%s_in_imag_%d_q%d', test_type, N, qbits); export_vector(fh, opt.bits, vstr, input_data_imag_q);
    vstr = sprintf('%s_ref_real_%d_q%d', test_type, N, qbits); export_vector(fh, opt.bits, vstr, ref_data_real_q);
    vstr = sprintf('%s_ref_imag_%d_q%d', test_type, N, qbits); export_vector(fh, opt.bits, vstr, ref_data_imag_q);
    fclose(fh);
    fprintf(1, 'Exported %s.\n', header_fn);
end
