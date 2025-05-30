% Export configuration blobs for Selector

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2025, Intel Corporation.

function sof_selector_blobs()

	% Common definitions
	str_config = "selector_config";
	str_exported = "Exported with script sof_selector_blobs.m";
	str_howto = "cd tools/tune/selector; octave sof_selector_blobs.m"
	sof_tools = '../../../../tools';
	sof_tplg = fullfile(sof_tools, 'topology');
	sof_tplg_selector = fullfile(sof_tplg, 'topology2/include/components/micsel');

	sof_selector_paths(true);

	% Matrix for 1:1 pass-through
	sel.rsvd0 = 0;
	sel.rsvd1 = 0;
	sel.coeffs = diag(ones(8, 1));
	blob8 = sof_selector_build_blob(sel);
	tplg2_fn = sprintf("%s/passthrough.conf", sof_tplg_selector);
	sof_check_create_dir(tplg2_fn);
	sof_tplg2_write(tplg2_fn, blob8, str_config, str_exported, str_howto);

	% 5.1 to stereo downmix, see ITU-R BS.775-4, Annex 4
	fl = 1;
	fr = 2;
	fc = 3;
	lfe = 4;
	sl = 5;
	sr = 6;
	m = zeros(8,8);
	m(1, fl) = 1.0000; m(1, fr) = 0.0000; m(1, fc) = 0.7071; m(1, sl) = 0.7071; m(1, sr) = 0.0000;
	m(2, fl) = 0.0000; m(2, fr) = 1.0000; m(2, fc) = 0.7071; m(2, sl) = 0.0000; m(2, sr) = 0.7071;
	m(1, lfe) = 10^(+4/20); % +10 dB, attenuate by -6 dB to left
	m(2, lfe) = 10^(+4/20); % +10 dB, attenuate by -6 dB to right
	max_sum_coef = max(sum(m, 2)); % Sum all coeffs for output channels, find max
	scale = 1 / max_sum_coef;
	sel.coeffs = scale .* m';
	sel
	blob8 = sof_selector_build_blob(sel);
	tplg2_fn = sprintf("%s/downmix_51_to_stereo.conf", sof_tplg_selector);
	sof_check_create_dir(tplg2_fn);
	sof_tplg2_write(tplg2_fn, blob8, str_config, str_exported, str_howto);

	% 5.1 to mono downmix, see ITU-R BS.775-4, Annex 4
	fl = 1;
	fr = 2;
	fc = 3;
	lfe = 4;
	sl = 5;
	sr = 6;
	m = zeros(8,8);
	m(1, fl) = 0.7071; m(1, fr) = 0.7071; m(1, fc) = 1.0000; m(1, sl) = 0.5000; m(1, sr) = 0.5000;
	m(1, lfe) = 10^(+10/20);
	max_sum_coef = max(sum(m, 2)); % Sum all coeffs for output channels, find max
	scale = 1 / max_sum_coef;
	sel.coeffs = scale .* m';
	sel
	blob8 = sof_selector_build_blob(sel);
	tplg2_fn = sprintf("%s/downmix_51_to_mono.conf", sof_tplg_selector);
	sof_check_create_dir(tplg2_fn);
	sof_tplg2_write(tplg2_fn, blob8, str_config, str_exported, str_howto);

	sof_selector_paths(false);
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
