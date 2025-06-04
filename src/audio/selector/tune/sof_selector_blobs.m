% Export configuration blobs for Selector

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2025, Intel Corporation.

function sof_selector_blobs()

	% See ITU-R BS.775-4 for mix coefficient values
	sof_selector_paths(true);

	% Matrix for 1:1 pass-through
	sel.rsvd0 = 0;
	sel.rsvd1 = 0;
	sel.coeffs = diag(ones(8, 1));
	write_blob(sel, "passthrough");

	% Stereo to mono downmix
	sel.coeffs = zeros(8,8);
	sel.coeffs(1, 1) = 0.7071;
	sel.coeffs(1, 2) = 0.7071;
	write_blob(sel, "downmix_stereo_to_mono");

	% 5.1 to stereo downmix
	fl = 1; fr = 2; fc = 3; lfe = 4; sl = 5; sr = 6;
	m = zeros(8,8);
	m(1, fl) = 1.0000; m(1, fr) = 0.0000; m(1, fc) = 0.7071; m(1, sl) = 0.7071; m(1, sr) = 0.0000;
	m(2, fl) = 0.0000; m(2, fr) = 1.0000; m(2, fc) = 0.7071; m(2, sl) = 0.0000; m(2, sr) = 0.7071;
	sel.coeffs = m;
	write_blob(sel, "downmix_51_to_stereo");
	sel.coeffs(1, lfe) = 10^(+4/20); % +10 dB, attenuate by -6 dB to left
	sel.coeffs(2, lfe) = 10^(+4/20); % +10 dB, attenuate by -6 dB to right
	write_blob(sel, "downmix_51_to_stereo_with_lfe");

	% 5.1 to mono downmix
	fl = 1; fr = 2; fc = 3; lfe = 4; sl = 5; sr = 6;
	m = zeros(8,8);
	m(1, fl) = 0.7071; m(1, fr) = 0.7071; m(1, fc) = 1.0000; m(1, sl) = 0.5000; m(1, sr) = 0.5000;
	sel.coeffs = m;
	write_blob(sel, "downmix_51_to_mono");
	sel.coeffs(1, lfe) = 10^(+10/20);
	write_blob(sel, "downmix_51_to_mono_with_lfe");

	% 7.1 to 5.1 downmix
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
	write_blob(sel, "downmix_71_to_51");

	% 7.1 to 5.1 downmix
	fl = 1; fr = 2; fc = 3; lfe = 4; bl = 5; br = 6;  sl = 7; sr = 8;
	m = zeros(8,8);
	m(1, fl) = 1.0000; m(1, fr) = 0.0000; m(1, fc) = 0.7071; m(1, sl) = 0.7071; m(1, sr) = 0.0000; m(1, bl) = 0.7071; m(1, br) = 0.0000;
	m(2, fl) = 0.0000; m(2, fr) = 1.0000; m(2, fc) = 0.7071; m(2, sl) = 0.0000; m(2, sr) = 0.7071; m(2, bl) = 0.0000; m(2, br) = 0.7071;
	sel.coeffs = m;
	write_blob(sel, "downmix_71_to_stereo");
	sel.coeffs(1, lfe) = 10^(+4/20); % +10 dB, attenuate by -6 dB to left
	sel.coeffs(2, lfe) = 10^(+4/20); % +10 dB, attenuate by -6 dB to right
	write_blob(sel, "downmix_71_to_stereo_with_lfe");

	% 7.1 to mono downmix
	fl = 1; fc = 3; fr = 2; sr = 8; br = 6; bl = 5; sl = 7; lfe = 4;
	m = zeros(8,8);
	m(1, fl) = 0.7071; m(1, fr) = 0.7071; m(1, fc) = 1.0000; m(1, sl) = 0.5000; m(1, sr) = 0.5000; m(1, bl) = 0.5000; m(1, br) = 0.5000;
	sel.coeffs = m;
	write_blob(sel, "downmix_71_to_mono");
	m(1, lfe) = 10^(+19/20); % +10 dB
	write_blob(sel, "downmix_71_to_mono_with_lfe");

	% mono to stereo upmix
	sel.coeffs = zeros(8,8);
	sel.coeffs(1, 1) = 10^(-3/20);
	sel.coeffs(2, 1) = 10^(-3/20);
	write_blob(sel, "upmix_mono_to_stereo");

	% mono to 5.1 / 7.1 upmix
	fc = 3
	sel.coeffs = zeros(8,8);
	sel.coeffs(fc, 1) = 1;
	write_blob(sel, "upmix_mono_to_51");
	write_blob(sel, "upmix_mono_to_71");

	% stereo to 5.1 / 7.1 upmix
	fl = 1; fr = 2;
	sel.coeffs = zeros(8,8);
	sel.coeffs(fl, 1) = 1;
	sel.coeffs(fr, 2) = 1;
	write_blob(sel, "upmix_stereo_to_51");
	write_blob(sel, "upmix_stereo_to_71");

	sof_selector_paths(false);
end

function write_blob(sel, blobname)
	str_config = "selector_config";
	str_exported = "Exported with script sof_selector_blobs.m";
	str_howto = "cd tools/tune/selector; octave sof_selector_blobs.m"
	sof_tools = '../../../../tools';
	sof_tplg = fullfile(sof_tools, 'topology');
	sof_tplg_selector = fullfile(sof_tplg, 'topology2/include/components/micsel');

	sel

	sum_coefs = sum(sel.coeffs, 2)'
	max_sum_coef = max(sum_coefs)
	if max_sum_coef > 1
		scale = 1 / max_sum_coef;
	else
		scale = 1;
	end

	scale
	sel.coeffs = scale .* sel.coeffs';

	blob8 = sof_selector_build_blob(sel);
	tplg2_fn = sprintf("%s/%s.conf", sof_tplg_selector, blobname);
	sof_check_create_dir(tplg2_fn);
	sof_tplg2_write(tplg2_fn, blob8, str_config, str_exported, str_howto);
end

function sof_selector_paths(enable)

	common = '../../../../tools/tune/common';
	if enable
		addpath(common);
	else
		rmpath(common);
	end
end

function blob8 = sof_selector_build_blob(sel)

	s = size(sel.coeffs);
	blob_type = 0;
	blob_param_id = 0; % IPC4_SELECTOR_COEFFS_CONFIG_ID
	data_length = s(1) * s(2) + 2;
	data_size = 2 * data_length; % int16_t matrix
	coeffs_vec = reshape(sel.coeffs, 1, []); % convert to row vector
	coeffs_q10 = int16(round(coeffs_vec .* 1024)); % Q6.10
	ipc_ver = 4;
	[abi_bytes, abi_size] = sof_get_abi(data_size, ipc_ver, blob_type, blob_param_id);
	blob_size = data_size + abi_size;
	blob8 = uint8(zeros(1, blob_size));
	blob8(1:abi_size) = abi_bytes;
	j = abi_size + 1;

	% header
	blob8(j:j+1) = int16_to_byte(int16(sel.rsvd0));
	j = j + 2;
	blob8(j:j+1) = int16_to_byte(int16(sel.rsvd1));
	j = j + 2;

	% coeffs matrix
	for i = 1:(s(1) * s(2))
		blob8(j:j+1) = int16_to_byte(coeffs_q10(i));
		j = j + 2;
	end
end

function bytes = int16_to_byte(word)
	sh = [0 -8];
	bytes = uint8(zeros(1,2));
	bytes(1) = bitand(bitshift(word, sh(1)), 255);
	bytes(2) = bitand(bitshift(word, sh(2)), 255);
end
