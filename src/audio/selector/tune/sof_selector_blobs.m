% sof_selector_blobs - export configuration blobs for Selector
%
% This script is run without arguments. It exports a number of
% configuration blobs for selector/micsel component. The first
% category of blobs are for upmix and downmix between mono to
% 7.1 channel audio formats.
%
% The second category is for duplicating stereo to four channels
% for 2-way speaker crossover filter.
%

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2025-2026, Intel Corporation.

function sof_selector_blobs()

	% See ITU-R BS.775-4 for mix coefficient values
	sof_selector_paths(true);

	% Values of enum ipc4_channel_config
	IPC4_CHANNEL_CONFIG_MONO = 0;
	IPC4_CHANNEL_CONFIG_STEREO = 1;
	IPC4_CHANNEL_CONFIG_QUATRO = 5;
	IPC4_CHANNEL_CONFIG_5_POINT_1 = 8;
	IPC4_CHANNEL_CONFIG_7_POINT_1 = 12;

	% Matrix for 1:1 pass-through
	sel.ch_count = [8 8]; % Number of channels
	sel.ch_config = [IPC4_CHANNEL_CONFIG_7_POINT_1 IPC4_CHANNEL_CONFIG_7_POINT_1];
	sel.coeffs = diag(ones(8, 1));
	passthrough_pack8 = write_blob(sel, "passthrough");

	% Stereo to mono downmix
	sel.ch_count = [2 1];
	sel.ch_config = [IPC4_CHANNEL_CONFIG_STEREO IPC4_CHANNEL_CONFIG_MONO];
	sel.coeffs = zeros(8,8);
	sel.coeffs(1, 1) = 0.7071;
	sel.coeffs(1, 2) = 0.7071;
	stereo_to_mono_pack8 = write_blob(sel, "downmix_stereo_to_mono");

	% 5.1 to stereo downmix
	sel.ch_count = [6 2];
	sel.ch_config = [IPC4_CHANNEL_CONFIG_5_POINT_1 IPC4_CHANNEL_CONFIG_STEREO];
	fl = 1; fr = 2; fc = 3; lfe = 4; sl = 5; sr = 6;
	m = zeros(8,8);
	m(1, fl) = 1.0000; m(1, fr) = 0.0000; m(1, fc) = 0.7071; m(1, sl) = 0.7071; m(1, sr) = 0.0000;
	m(2, fl) = 0.0000; m(2, fr) = 1.0000; m(2, fc) = 0.7071; m(2, sl) = 0.0000; m(2, sr) = 0.7071;
	sel.coeffs = m;
	sel.coeffs(1, lfe) = 10^(+4/20); % +10 dB, attenuate by -6 dB to left
	sel.coeffs(2, lfe) = 10^(+4/20); % +10 dB, attenuate by -6 dB to right
	sixch_to_stereo_pack8 = write_blob(sel, "downmix_51_to_stereo_with_lfe");

	% 5.1 to mono downmix
	sel.ch_count = [6 1];
	sel.ch_config = [IPC4_CHANNEL_CONFIG_5_POINT_1 IPC4_CHANNEL_CONFIG_MONO];
	fl = 1; fr = 2; fc = 3; lfe = 4; sl = 5; sr = 6;
	m = zeros(8,8);
	m(1, fl) = 0.7071; m(1, fr) = 0.7071; m(1, fc) = 1.0000; m(1, sl) = 0.5000; m(1, sr) = 0.5000;
	sel.coeffs = m;
	sel.coeffs(1, lfe) = 10^(+10/20);
	sixch_to_mono_pack8 = write_blob(sel, "downmix_51_to_mono_with_lfe");

	% 7.1 to 5.1 downmix
	sel.ch_count = [8 6];
	sel.ch_config = [IPC4_CHANNEL_CONFIG_7_POINT_1 IPC4_CHANNEL_CONFIG_5_POINT_1];
	fl8 = 1; fr8 = 2; fc8 = 3; lfe8 = 4; bl8 = 5; br8 = 6;	sl8 = 7; sr8 = 8;
	fl6 = 1; fr6 = 2; fc6 = 3; lfe6 = 4; sl6 = 5; sr6 = 6;
	m = zeros(8,8);
	m(fl6, fl8) = 1;
	m(fr6, fr8) = 1;
	m(fc6, fc8) = 1;
	m(sl6, sl8) = 1;
	m(sr6, sr8) = 1;
	m(sl6, bl8) = 1;
	m(sr6, br8) = 1;
	m(lfe6, lfe8) = 1;
	sel.coeffs = m;
	eightch_to_sixch_pack8 = write_blob(sel, "downmix_71_to_51");

	% 7.1 to stereo downmix
	sel.ch_count = [8 2];
	sel.ch_config = [IPC4_CHANNEL_CONFIG_7_POINT_1 IPC4_CHANNEL_CONFIG_STEREO];
	fl = 1; fr = 2; fc = 3; lfe = 4; bl = 5; br = 6;  sl = 7; sr = 8;
	m = zeros(8,8);
	m(1, fl) = 1.0000; m(1, fr) = 0.0000; m(1, fc) = 0.7071; m(1, sl) = 0.7071; m(1, sr) = 0.0000; m(1, bl) = 0.7071; m(1, br) = 0.0000;
	m(2, fl) = 0.0000; m(2, fr) = 1.0000; m(2, fc) = 0.7071; m(2, sl) = 0.0000; m(2, sr) = 0.7071; m(2, bl) = 0.0000; m(2, br) = 0.7071;
	sel.coeffs = m;
	sel.coeffs(1, lfe) = 10^(+4/20); % +10 dB, attenuate by -6 dB to left
	sel.coeffs(2, lfe) = 10^(+4/20); % +10 dB, attenuate by -6 dB to right
	eightch_to_stereo_pack8 = write_blob(sel, "downmix_71_to_stereo_with_lfe");

	% 7.1 to mono downmix
	sel.ch_count = [8 1];
	sel.ch_config = [IPC4_CHANNEL_CONFIG_7_POINT_1 IPC4_CHANNEL_CONFIG_MONO];
	fl = 1; fc = 3; fr = 2; sr = 8; br = 6; bl = 5; sl = 7; lfe = 4;
	m = zeros(8,8);
	m(1, fl) = 0.7071; m(1, fr) = 0.7071; m(1, fc) = 1.0000; m(1, sl) = 0.5000; m(1, sr) = 0.5000; m(1, bl) = 0.5000; m(1, br) = 0.5000;
	sel.coeffs = m;
	m(1, lfe) = 10^(+19/20); % +10 dB
	eightch_to_mono_pack8 = write_blob(sel, "downmix_71_to_mono_with_lfe");

	% mono to stereo upmix
	sel.ch_count = [1 2];
	sel.ch_config = [IPC4_CHANNEL_CONFIG_MONO IPC4_CHANNEL_CONFIG_STEREO];
	sel.coeffs = zeros(8,8);
	sel.coeffs(1, 1) = 10^(-3/20);
	sel.coeffs(2, 1) = 10^(-3/20);
	mono_to_stereo_pack8 = write_blob(sel, "upmix_mono_to_stereo");

	% mono to 5.1 / 7.1 upmix
	sel.ch_count = [1 6];
	sel.ch_config = [IPC4_CHANNEL_CONFIG_MONO IPC4_CHANNEL_CONFIG_5_POINT_1];
	fc = 3;
	sel.coeffs = zeros(8,8);
	sel.coeffs(fc, 1) = 1;
	mono_to_sixch_pack8 = write_blob(sel, "upmix_mono_to_51");
	sel.ch_count = [1 8];
	sel.ch_config = [IPC4_CHANNEL_CONFIG_MONO IPC4_CHANNEL_CONFIG_7_POINT_1];
	mono_to_eightch_pack8 = write_blob(sel, "upmix_mono_to_71");

	% stereo to 5.1 / 7.1 upmix
	sel.ch_count = [2 6];
	sel.ch_config = [IPC4_CHANNEL_CONFIG_STEREO IPC4_CHANNEL_CONFIG_5_POINT_1];
	fl = 1; fr = 2;
	sel.coeffs = zeros(8,8);
	sel.coeffs(fl, 1) = 1;
	sel.coeffs(fr, 2) = 1;
	stereo_to_sixch_pack8 = write_blob(sel, "upmix_stereo_to_51");
	sel.ch_count = [2 8];
	sel.ch_config = [IPC4_CHANNEL_CONFIG_STEREO IPC4_CHANNEL_CONFIG_7_POINT_1];
	stereo_to_eightch_pack8 = write_blob(sel, "upmix_stereo_to_71");

	% For blob format with multiple up/down-mix profiles, intended
	% for playback decoder offload conversions.
	multi_pack8 = [passthrough_pack8 mono_to_stereo_pack8 sixch_to_stereo_pack8 eightch_to_stereo_pack8];
	write_8bit_packed(multi_pack8, 'stereo_endpoint_playback_updownmix');

	% Stereo to L,L,R,R
	sel.ch_count = [2 4];
	sel.ch_config = [IPC4_CHANNEL_CONFIG_STEREO IPC4_CHANNEL_CONFIG_QUATRO];
	sel.coeffs = [ 1 0 0 0 0 0 0 0 ; ...
		       1 0 0 0 0 0 0 0 ; ...
		       0 1 0 0 0 0 0 0 ; ...
		       0 1 0 0 0 0 0 0 ; ...
		       0 0 0 0 0 0 0 0 ; ...
		       0 0 0 0 0 0 0 0 ; ...
		       0 0 0 0 0 0 0 0 ; ...
		       0 0 0 0 0 0 0 0 ];
	write_blob(sel, "xover_selector_lr_to_llrr");

	% Stereo to R,R,L,L
	sel.ch_count = [2 4];
	sel.ch_config = [IPC4_CHANNEL_CONFIG_STEREO IPC4_CHANNEL_CONFIG_QUATRO];
	sel.coeffs = [ 0 1 0 0 0 0 0 0 ; ...
		       0 1 0 0 0 0 0 0 ; ...
		       1 0 0 0 0 0 0 0 ; ...
		       1 0 0 0 0 0 0 0 ; ...
		       0 0 0 0 0 0 0 0 ; ...
		       0 0 0 0 0 0 0 0 ; ...
		       0 0 0 0 0 0 0 0 ; ...
		       0 0 0 0 0 0 0 0 ];
	write_blob(sel, "xover_selector_lr_to_rrll");

	% Stereo to L,R,L,R
	sel.ch_count = [2 4];
	sel.ch_config = [IPC4_CHANNEL_CONFIG_STEREO IPC4_CHANNEL_CONFIG_QUATRO];
	sel.coeffs = [ 1 0 0 0 0 0 0 0 ; ...
		       0 1 0 0 0 0 0 0 ; ...
		       1 0 0 0 0 0 0 0 ; ...
		       0 1 0 0 0 0 0 0 ; ...
		       0 0 0 0 0 0 0 0 ; ...
		       0 0 0 0 0 0 0 0 ; ...
		       0 0 0 0 0 0 0 0 ; ...
		       0 0 0 0 0 0 0 0 ];
	write_blob(sel, "xover_selector_lr_to_lrlr");

	sof_selector_paths(false);
end

function pack8 = write_blob(sel, blobname)
	pack8 = pack_selector_config(sel);
	pack8_copy = pack8;
	pack8_copy(1:4) = uint8([0 0 0 0]);
	write_8bit_packed(pack8_copy, blobname);
end

function write_8bit_packed(pack8, blobname)
	blob8 = sof_selector_build_blob(pack8);
	str_config = "selector_config";
	str_exported = "Exported with script sof_selector_blobs.m";
	str_howto = "cd tools/tune/selector; octave sof_selector_blobs.m";
	sof_tools = '../../../../tools';
	sof_tplg = fullfile(sof_tools, 'topology');
	sof_tplg_selector = fullfile(sof_tplg, 'topology2/include/components/micsel');
	tplg2_fn = sprintf("%s/%s.conf", sof_tplg_selector, blobname);
	sof_check_create_dir(tplg2_fn);
	sof_tplg2_write(tplg2_fn, blob8, str_config, str_exported, str_howto);
end

function pack8 = pack_selector_config(sel)

	sum_coefs = sum(sel.coeffs, 2)';
	max_sum_coef = max(sum_coefs);
	if max_sum_coef > 1
		scale = 1 / max_sum_coef;
	else
		scale = 1;
	end

	sel.coeffs = scale .* sel.coeffs';
	coeffs_vec = reshape(sel.coeffs, 1, []); % convert to row vector
	coeffs_q10 = int16(round(coeffs_vec .* 1024)); % Q6.10
	pack8 = uint8(zeros(1, 2 * length(coeffs_q10) + 4));

	% header
	j = 1;
	pack8(j:j + 1) = uint8(sel.ch_count);
	j = j + 2;
	pack8(j:j + 1) = uint8(sel.ch_config);
	j = j + 2;

	% coeffs matrix
	for i = 1:length(coeffs_q10)
		pack8(j:j+1) = int16_to_byte(coeffs_q10(i));
		j = j + 2;
	end

   end

function sof_selector_paths(enable)

	common = '../../../../tools/tune/common';
	if enable
		addpath(common);
	else
		rmpath(common);
	end
end

function blob8 = sof_selector_build_blob(pack8)

	blob_type = 0;
	blob_param_id = 0; % IPC4_SELECTOR_COEFFS_CONFIG_ID
	data_size = length(pack8);
	ipc_ver = 4;
	[abi_bytes, abi_size] = sof_get_abi(data_size, ipc_ver, blob_type, blob_param_id);
	blob_size = data_size + abi_size;
	blob8 = uint8(zeros(1, blob_size));
	blob8(1:abi_size) = abi_bytes;
	j = abi_size + 1;

	blob8(j:j+data_size-1) = pack8;
end

function bytes = int16_to_byte(word)
	sh = [0 -8];
	bytes = uint8(zeros(1,2));
	bytes(1) = bitand(bitshift(word, sh(1)), 255);
	bytes(2) = bitand(bitshift(word, sh(2)), 255);
end
