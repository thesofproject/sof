% Export configuration blobs for Level Multiplier
%
% This script creates configuration blobs for the Level Multiplier
% component to apply gains -40, -30, -20, -10, 0, 10, 20, 30, 40 dB.
% Run the script interactively from Octave shell with just command:
%
% sof_level_multiplier_blobs
%
% There are no arguments for the function. Or from SOF level directory
% with command:
%
% cd src/audio/level_multiplier/tune; octave sof_level_multiplier_blobs.m
%

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2025, Intel Corporation.

function sof_level_multiplier_blobs()

	% Set the parameters here
	sof_tools = '../../../../tools';
	sof_tplg = fullfile(sof_tools, 'topology/topology2');
	sof_tplg_level_multiplier = fullfile(sof_tplg, 'include/components/level_multiplier');
	sof_ctl_level_multiplier = fullfile(sof_tools, 'ctl/ipc4/level_multiplier');

	sof_level_multiplier_paths(true);

	for param = -40:10:40
		gain_value = sof_level_multiplier_db2lin(param);
		blob8 = sof_level_multiplier_build_blob(gain_value);
		tplg2_fn = sprintf("%s/gain_%d_db.conf", sof_tplg_level_multiplier, param);
		sof_tplg2_write(tplg2_fn, blob8, "level_multiplier_config", ...
				"Exported with script sof_level_multiplier_blobs.m" , ...
				"cd tools/tune/level_multiplier; octave sof_level_multiplier_blobs.m");
		ctl_fn = sprintf("%s/gain_%d_db.txt", sof_ctl_level_multiplier, param);
		sof_alsactl_write(ctl_fn, blob8);
	end

	sof_level_multiplier_paths(false);
end

function lin_value = sof_level_multiplier_db2lin(db)
	scale = 2^23;
	lin_value = int32(10^(db/20) * scale);
end

function sof_level_multiplier_paths(enable)

	common = '../../../../tools/tune/common';
	if enable
		addpath(common);
	else
		rmpath(common);
	end
end

function blob8 = sof_level_multiplier_build_blob(param_values)

	blob_type = 0;
	blob_param_id = 1;
	data_length = length(param_values);
	data_size = 4 * data_length;
	ipc_ver = 4;
	[abi_bytes, abi_size] = sof_get_abi(data_size, ipc_ver, blob_type, blob_param_id);
	blob_size = data_size + abi_size;
	blob8 = uint8(zeros(1, blob_size));
	blob8(1:abi_size) = abi_bytes;
	j = abi_size + 1;
	for i = 1:data_length
		blob8(j:j+3) = word2byte(param_values(i));
		j=j+4;
	end
end

function bytes = word2byte(word)
        sh = [0 -8 -16 -24];
	bytes = uint8(zeros(1,4));
	bytes(1) = bitand(bitshift(word, sh(1)), 255);
	bytes(2) = bitand(bitshift(word, sh(2)), 255);
	bytes(3) = bitand(bitshift(word, sh(3)), 255);
	bytes(4) = bitand(bitshift(word, sh(4)), 255);
end
