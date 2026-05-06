% gen_ztest_blobs.m - Generate configuration blobs for audio module ZTests
%
% Run from sof/zephyr/test/audio/ with:
%   octave --no-window-system gen_ztest_blobs.m
%
% Generates C header files in blobs/ directory containing config data
% for modules that require valid coefficient blobs during processing.

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright(c) 2026 Intel Corporation.

function gen_ztest_blobs()

	fprintf(1, 'Generating ZTest configuration blobs...\n');

	blobs_dir = 'blobs';
	if ~exist(blobs_dir, 'dir')
		mkdir(blobs_dir);
	end

	gen_eq_iir_blob(blobs_dir);
	gen_eq_fir_blob(blobs_dir);
	gen_multiband_drc_blob(blobs_dir);
	gen_tdfb_blob(blobs_dir);

	fprintf(1, 'Done. All blob headers generated in %s/\n', blobs_dir);
end

%% ========================================================================
%% EQ IIR: Simple 2-channel passthrough (flat response) blob
%% ========================================================================
function gen_eq_iir_blob(blobs_dir)

	fprintf(1, '  Generating EQ IIR passthrough blob...\n');

	% Add paths
	eq_tune = '../../../src/audio/eq_iir/tune';
	common = '../../../tools/tune/common';
	addpath(eq_tune);
	addpath(common);

	% Design a flat passthrough IIR: single biquad with b=[1,0,0], a=[1,0,0]
	% This is a unity gain allpass - input passes through unchanged
	% Use the sof_eq_iir_blob_quant with identity zpk
	z = [];  % No zeros (allpass)
	p = [];  % No poles (allpass)
	k = 1.0; % Unity gain
	bq = sof_eq_iir_blob_quant(z, p, k);

	% Build blob for 2 channels, same response for both
	channels_in_config = 2;
	assign_response = [0 0];
	num_responses = 1;
	bm = sof_eq_iir_blob_merge(channels_in_config, num_responses, ...
				    assign_response, bq);

	% Pack into IPC4 format blob
	bp = sof_eq_iir_blob_pack(bm, 4);

	% Export as C header
	fn = fullfile(blobs_dir, 'eq_iir_passthrough_2ch.h');
	export_c_blob(fn, bp, 'eq_iir_blob', ...
		      'EQ IIR passthrough config for 2 channels');

	rmpath(eq_tune);
	rmpath(common);
end

%% ========================================================================
%% EQ FIR: Simple 2-channel passthrough (single tap unity) blob
%% ========================================================================
function gen_eq_fir_blob(blobs_dir)

	fprintf(1, '  Generating EQ FIR passthrough blob...\n');

	% Add paths
	eq_tune = '../../../src/audio/eq_iir/tune';
	common = '../../../tools/tune/common';
	addpath(eq_tune);
	addpath(common);

	% Design a unity passthrough FIR: single coefficient = 1.0
	b_pass = [1];
	bq = sof_eq_fir_blob_quant(b_pass);

	% Build blob for 2 channels, same response for both
	channels_in_config = 2;
	assign_response = [0 0];
	num_responses = 1;
	bm = sof_eq_fir_blob_merge(channels_in_config, num_responses, ...
				    assign_response, bq);

	% Pack into IPC4 format blob
	bp = sof_eq_fir_blob_pack(bm, 4);

	% Export as C header
	fn = fullfile(blobs_dir, 'eq_fir_passthrough_2ch.h');
	export_c_blob(fn, bp, 'eq_fir_blob', ...
		      'EQ FIR passthrough config for 2 channels');

	rmpath(eq_tune);
	rmpath(common);
end

%% ========================================================================
%% Multiband DRC: 2-band passthrough (DRC disabled) blob
%% ========================================================================
function gen_multiband_drc_blob(blobs_dir)

	fprintf(1, '  Generating Multiband DRC passthrough blob...\n');

	% Add paths - multiband_drc depends on crossover, drc, and eq_iir tune
	mbdrc_tune = '../../../src/audio/multiband_drc/tune';
	crossover_tune = '../../../src/audio/crossover/tune';
	drc_tune = '../../../src/audio/drc/tune';
	eq_tune = '../../../src/audio/eq_iir/tune';
	common = '../../../tools/tune/common';
	addpath(mbdrc_tune);
	addpath(crossover_tune);
	addpath(drc_tune);
	addpath(eq_tune);
	addpath(common);

	% Use the passthrough config from sof_example_multiband_drc.m
	% This creates a 2-band DRC with all bands disabled
	sample_rate = 48000;
	nyquist = sample_rate / 2;
	num_bands = 2;
	enable_emp_deemp = 0;

	% IIR emphasis params (not used when enable_emp_deemp = 0)
	iir_params.stage_gain = 0.01;
	iir_params.stage_ratio = 2;
	iir_params.anchor = 15000 / nyquist;

	% DRC params - all disabled for passthrough
	for i = 1:4
		drc_params(i).enabled = 0;
		drc_params(i).threshold = -30;
		drc_params(i).knee = 20;
		drc_params(i).ratio = 10;
		drc_params(i).attack = 0.003;
		drc_params(i).release = 0.2;
		drc_params(i).pre_delay = 0.006;
		drc_params(i).release_zone = [0.09 0.16 0.42 0.98];
		drc_params(i).release_spacing = 5;
		drc_params(i).post_gain = 0;
		drc_params(i).band_lower_freq = [0 2000 8000 16000](i) / nyquist;
	end

	% Generate coefficient structures
	[emp_coefs, deemp_coefs] = sof_iir_gen_quant_coefs(iir_params, sample_rate, enable_emp_deemp);
	crossover_coefs = sof_crossover_gen_quant_coefs(num_bands, sample_rate, ...
							drc_params(2).band_lower_freq, ...
							drc_params(3).band_lower_freq, ...
							drc_params(4).band_lower_freq);
	drc_coefs = sof_drc_gen_quant_coefs(num_bands, sample_rate, drc_params);

	% Build IPC4 blob
	blob8 = sof_multiband_drc_build_blob(num_bands, enable_emp_deemp, emp_coefs, ...
					     deemp_coefs, crossover_coefs, drc_coefs, ...
					     'little', 4);

	% Export as C header
	fn = fullfile(blobs_dir, 'multiband_drc_passthrough.h');
	export_c_blob(fn, blob8, 'multiband_drc_blob', ...
		      'Multiband DRC passthrough config (2-band, DRC disabled)');

	rmpath(mbdrc_tune);
	rmpath(crossover_tune);
	rmpath(drc_tune);
	rmpath(eq_tune);
	rmpath(common);
end

%% ========================================================================
%% TDFB: 2-channel passthrough (no beamforming) blob
%% ========================================================================
function gen_tdfb_blob(blobs_dir)

	fprintf(1, '  Generating TDFB passthrough blob...\n');

	% Add paths
	tdfb_tune = '../../../src/audio/tdfb/tune';
	eq_tune = '../../../src/audio/eq_iir/tune';
	common = '../../../tools/tune/common';
	addpath(tdfb_tune);
	addpath(eq_tune);
	addpath(common);

	% Use the 2-channel passthrough config from sof_example_pass_config.m
	bf = sof_bf_defaults();
	bf.input_channel_select = [0 1];
	bf.output_channel_mix   = [1 2];
	bf.output_stream_mix    = [0 0];
	bf.num_output_channels  = 2;
	bf.num_output_streams   = 1;
	bf.beam_off_defined     = 0;
	bf.num_angles           = 1;
	bf.steer_az             = 0;
	bf.steer_el             = 0;
	bf.num_filters          = 2;

	% Two FIR filters with first tap set to one (passthrough)
	bf.w = [1 0 0 0; 1 0 0 0]';

	% Don't export to default file locations
	bf.sofctl3_fn = '';
	bf.sofctl4_fn = '';
	bf.tplg1_fn = '';
	bf.tplg2_fn = '';
	bf.ucmbin3_fn = '';
	bf.ucmbin4_fn = '';

	% Quantize filters manually (like sof_bf_export does)
	filters = [];
	for j = 1:bf.num_angles
		for i = 1:bf.num_filters
			coefs = squeeze(bf.w(:,i,j));
			bq = sof_eq_fir_blob_quant(coefs, 16, 0);
			filters = [filters bq];
		end
	end
	bf.all_filters = filters;

	% Build IPC4 blob
	bp4 = sof_bf_blob_pack(bf, 4);

	% Export as C header
	fn = fullfile(blobs_dir, 'tdfb_passthrough_2ch.h');
	export_c_blob(fn, bp4, 'tdfb_blob', ...
		      'TDFB passthrough config for 2 channels');

	rmpath(tdfb_tune);
	rmpath(eq_tune);
	rmpath(common);
end

%% ========================================================================
%% Helper: Export blob8 array as C uint32_t header file
%% ========================================================================
function export_c_blob(fn, blob8, varname, description)

	fh = fopen(fn, 'w');
	if fh < 0
		error('Could not open file: %s', fn);
	end

	fprintf(fh, '/* SPDX-License-Identifier: BSD-3-Clause\n');
	fprintf(fh, ' *\n');
	fprintf(fh, ' * Copyright(c) 2026 Intel Corporation.\n');
	fprintf(fh, ' *\n');
	fprintf(fh, ' * %s\n', description);
	fprintf(fh, ' * Auto-generated by gen_ztest_blobs.m - do not edit.\n');
	fprintf(fh, ' */\n\n');
	fprintf(fh, '#ifndef ZTEST_BLOB_%s_H\n', upper(varname));
	fprintf(fh, '#define ZTEST_BLOB_%s_H\n\n', upper(varname));
	fprintf(fh, '#include <stdint.h>\n\n');

	% Strip 8-byte TLV header (same as sof_export_c_eq_uint32t with justeq=0)
	blob8 = blob8(9:end);

	% Pad to multiple of 4
	n_orig = length(blob8);
	n_new = ceil(n_orig / 4);
	blob8_padded = zeros(1, n_new * 4, 'uint8');
	blob8_padded(1:n_orig) = blob8;

	% Convert to uint32
	blob32 = zeros(1, n_new, 'uint32');
	k = 2.^[0 8 16 24];
	for i = 1:n_new
		j = (i - 1) * 4;
		blob32(i) = uint32(blob8_padded(j+1)) * k(1) + ...
			    uint32(blob8_padded(j+2)) * k(2) + ...
			    uint32(blob8_padded(j+3)) * k(3) + ...
			    uint32(blob8_padded(j+4)) * k(4);
	end

	% Write C array
	numbers_per_line = 4;
	fprintf(fh, 'static const uint32_t %s[%d] = {\n', varname, n_new);

	for i = 1:n_new
		if mod(i - 1, numbers_per_line) == 0
			fprintf(fh, '\t');
		end
		fprintf(fh, '0x%08x', blob32(i));
		if i < n_new
			fprintf(fh, ',');
		end
		if mod(i, numbers_per_line) == 0 || i == n_new
			fprintf(fh, '\n');
		else
			fprintf(fh, ' ');
		end
	end

	fprintf(fh, '};\n\n');
	fprintf(fh, '#endif /* ZTEST_BLOB_%s_H */\n', upper(varname));
	fclose(fh);
	fprintf(1, '    -> %s (%d uint32 words)\n', fn, n_new);
end
