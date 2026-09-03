% Export configuration blobs for Sound Dose

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2025, Intel Corporation.

function sof_sound_dose_blobs()

	% Common definitions
	sof_tools = '../../../../tools';
	sof_tplg = fullfile(sof_tools, 'topology');
	fn.tpath = fullfile(sof_tplg, 'topology2/include/components/sound_dose');
	str_comp = "sound_dose_config";
	str_comment = "Exported with script sof_sound_dose_blobs.m";
	str_howto = "cd src/audio/sound_dose/tune; octave sof_sound_dose_blobs.m";
	bytes_control_param_id = 202;

	% Set the parameters here
	sof_tools = '../../../../tools';
	sof_tplg = fullfile(sof_tools, 'topology');
	sof_tplg_sound_dose = fullfile(sof_tplg, 'topology2/include/components/sound_dose');
	sof_ctl_sound_dose = fullfile(sof_tools, 'ctl/ipc4/sound_dose');

	sof_sound_dose_paths(true);

	blob8 = sof_sound_dose_build_blob([10000 0], 0);
	tplg2_fn = sprintf("%s/setup_sens_100db.conf", sof_tplg_sound_dose);
	sof_tplg2_write(tplg2_fn, blob8, str_comp, str_comment, str_howto);
	sof_alsactl_write([sof_ctl_sound_dose "/setup_sens_100db.txt"], blob8);

	blob8 = sof_sound_dose_build_blob([0 0], 0);
	tplg2_fn = sprintf("%s/setup_sens_0db.conf", sof_tplg_sound_dose);
	sof_tplg2_write(tplg2_fn, blob8, str_comp, str_comment, str_howto);
	sof_alsactl_write([sof_ctl_sound_dose "/setup_sens_0db.txt"], blob8);

	blob8 = sof_sound_dose_build_blob([0 0], 1);
	tplg2_fn = sprintf("%s/setup_vol_0db.conf", sof_tplg_sound_dose);
	sof_tplg2_write(tplg2_fn, blob8, str_comp, str_comment, str_howto);
	sof_alsactl_write([sof_ctl_sound_dose "/setup_vol_0db.txt"], blob8);

	blob8 = sof_sound_dose_build_blob([-1000 0], 1);
	tplg2_fn = sprintf("%s/setup_vol_-10db.conf", sof_tplg_sound_dose);
	sof_tplg2_write(tplg2_fn, blob8, str_comp, str_comment, str_howto);
	sof_alsactl_write([sof_ctl_sound_dose "/setup_vol_-10db.txt"], blob8);

	blob8 = sof_sound_dose_build_blob([-1000 0], 2);
	tplg2_fn = sprintf("%s/setup_gain_-10db.conf", sof_tplg_sound_dose);
	sof_tplg2_write(tplg2_fn, blob8, str_comp, str_comment, str_howto);
	sof_alsactl_write([sof_ctl_sound_dose "/setup_gain_-10db.txt"], blob8);

	blob8 = sof_sound_dose_build_blob([0 0], 2);
	tplg2_fn = sprintf("%s/setup_gain_0db.conf", sof_tplg_sound_dose);
	sof_tplg2_write(tplg2_fn, blob8, str_comp, str_comment, str_howto);
	sof_alsactl_write([sof_ctl_sound_dose "/setup_gain_0db.txt"], blob8);

	blob8 = sof_sound_dose_build_blob([], bytes_control_param_id);
	tplg2_fn = sprintf("%s/setup_data_init.conf", sof_tplg_sound_dose);
	sof_tplg2_write(tplg2_fn, blob8, str_comp, str_comment, str_howto);

	sof_sound_dose_paths(false);
end

function sof_sound_dose_paths(enable)

	common = '../../../../tools/tune/common';
	if enable
		addpath(common);
	else
		rmpath(common);
	end
end

function blob8 = sof_sound_dose_build_blob(param_values, blob_param_id)

	blob_type = 0;
	data_length = length(param_values);
	data_size = 2 * data_length;
	ipc_ver = 4;
	[abi_bytes, abi_size] = sof_get_abi(data_size, ipc_ver, blob_type, blob_param_id);
	blob_size = data_size + abi_size;
	blob8 = uint8(zeros(1, blob_size));
	blob8(1:abi_size) = abi_bytes;
	j = abi_size + 1;
	for i = 1:data_length
		blob8(j:j+1) = short2byte(int16(param_values(i)));
		j=j+2;
	end
end

function bytes = short2byte(word)
	sh = [0 -8];
	bytes = uint8(zeros(1,2));
	bytes(1) = bitand(bitshift(word, sh(1)), 255);
	bytes(2) = bitand(bitshift(word, sh(2)), 255);
end
