% ref_auditory - Generate C header files for auditory library unit tests

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright(c) 2022 Intel Corporation. All rights reserved.

function ref_auditory()

	path(path(), '../../../m');
	opt.describe = export_get_git_describe();

	%% Test 0, Mel <-> Hz conversions
	get_ref_hz_to_mel(opt, 'ref_hz', 'ref_mel', 'ref_revhz', 'ref_hz_to_mel.h');

	%% Test 1, largest input, Slaney off, Mel dB
	opt.test_n = 1;
	opt.input = 'ones';
	opt.num_mel_bins = 23;
	opt.fft_size = 512;
	opt.norm_slaney = 0;
	opt.mel_log = 'MEL_DB';
	opt.shift = 0;
	opt.bits = 16;
	get_ref_mel_filterbank(opt);
	opt.bits = 32;
	get_ref_mel_filterbank(opt);

	%% Test 2, random input, Slaney norm, Mel log
	opt.test_n = 2;
	opt.input = 'random';
	opt.num_mel_bins = 23;
	opt.fft_size = 512;
	opt.norm_slaney = 1;
	opt.mel_log = 'MEL_LOG';
	opt.shift = 1;
	opt.bits = 16;
	get_ref_mel_filterbank(opt);
	opt.bits = 32;
	get_ref_mel_filterbank(opt);

	%% Test 3, random input, Slaney off, Mel log10, 128 size, 10 bands
	opt.test_n = 3;
	opt.input = 'random';
	opt.num_mel_bins = 10;
	opt.fft_size = 128;
	opt.norm_slaney = 0;
	opt.mel_log = 'MEL_LOG10';
	opt.shift = 2;
	opt.bits = 16;
	get_ref_mel_filterbank(opt);
	opt.bits = 32;
	get_ref_mel_filterbank(opt);

	%% Test 4, random input, Slaney norm, Mel dB,1024 size, 23 bands
	opt.test_n = 4;
	opt.input = 'random';
	opt.num_mel_bins = 23;
	opt.fft_size = 1024;
	opt.norm_slaney = 1;
	opt.mel_log = 'MEL_DB';
	opt.shift = 3;
	opt.bits = 16;
	get_ref_mel_filterbank(opt);
	opt.bits = 32;
	get_ref_mel_filterbank(opt);

end

function get_ref_mel_filterbank(opt)

	header_fn = sprintf('ref_mel_filterbank_%d_test%d.h', opt.bits, opt.test_n);

	%% Initialize filterbank
	param.sample_frequency = 16e3;
	param.high_freq = 7.5e3;
	param.low_freq = 100;
	param.num_mel_bins = opt.num_mel_bins;
	param.pmin = 1e-9;
	param.norm_slaney = opt.norm_slaney;
	param.mel_log = opt.mel_log;
	param.top_db = 100;
	state.fft_padded_size = opt.fft_size;
	state.half_fft_size = state.fft_padded_size / 2 + 1;
	state = mfcc_get_mel_filterbank(param, state);

	%% Data to process
	switch lower(opt.input)
		case 'random'
			real = 2 * rand(state.half_fft_size, 1) - 1;
			imag = 2 * rand(state.half_fft_size, 1) - 1;
		case 'ones'
			real = ones(state.half_fft_size, 1);
			imag = ones(state.half_fft_size, 1);
		otherwise
			error('Illegal input');
	end

	switch opt.bits
		case 16
			[q_real, f_real] = export_quant_qxy(real, 16, 15, true); % Q1.15
			[q_imag, f_imag] = export_quant_qxy(imag, 16, 15, true); % Q1.15
		case 32
			[q_real, f_real] = export_quant_qxy(real, 32, 31, true); % Q1.31
			[q_imag, f_imag] = export_quant_qxy(imag, 32, 31, true); % Q1.31
		otherwise
			error('Illegal bits value');
	end

	s_half = complex(f_real, f_imag);
	mel_log = mfcc_power_to_mel_log(state, param, s_half, opt.shift);
	q_mel_log = export_quant_qxy(mel_log, 16, 7); %Q8.7

	define_prefix = sprintf('MEL_FILTERBANK_%d_TEST%d_', opt.bits, opt.test_n);
	vector_prefix = sprintf('mel_filterbank_%d_test%d_', opt.bits, opt.test_n);

	fh = export_headerfile_open(header_fn);
	comment = sprintf('Created %s with script ref_matrix.m %s', ...
			  datestr(now, 0), opt.describe);
	export_comment(fh, comment);
	export_ndefine(fh, [define_prefix 'FFT_SIZE'], opt.fft_size);
	export_ndefine(fh, [define_prefix 'NUM_MEL_BINS'], opt.num_mel_bins);
	export_ndefine(fh, [define_prefix 'NORM_SLANEY'], opt.norm_slaney);
	export_sdefine(fh, [define_prefix 'MEL_LOG'], opt.mel_log);
	export_ndefine(fh, [define_prefix 'SHIFT'], opt.shift);

	export_vector(fh, opt.bits, [vector_prefix 'real'], q_real);
	export_vector(fh, opt.bits, [vector_prefix 'imag'], q_imag);
	export_vector(fh, 16, [vector_prefix 'mel_log'], q_mel_log);
	fclose(fh);
	fprintf(1, 'Exported %s.\n', header_fn);

end

function get_ref_hz_to_mel(opt, hz_name, mel_name, rev_hz_name, header_fn)

	n = 100;
	hz_min = 0;
	hz_max = 32767;

	hz = linspace(hz_min, hz_max, n);
	mel = mfcc_hz_to_mel(hz);
	[qhz, fqhz] = export_quant_qxy(hz, 16, 0); % Q15.0
	[qmel, fqmel] = export_quant_qxy(mel, 16, 2); % Q14.2
	revhz = mfcc_mel_to_hz(fqmel);
	qrevhz = export_quant_qxy(hz, 16, 0); % Q16.0

	fh = export_headerfile_open(header_fn);
	comment = sprintf('Created %s with script ref_matrix.m %s', ...
			  datestr(now, 0), opt.describe);
	export_comment(fh, comment);
	export_ndefine(fh, 'HZ_TO_MEL_NPOINTS', n);
	export_vector(fh, 16, hz_name, qhz);
	export_vector(fh, 16, mel_name, qmel);
	export_vector(fh, 16, rev_hz_name, qrevhz);
	fclose(fh);
	fprintf(1, 'Exported %s.\n', header_fn);

end
