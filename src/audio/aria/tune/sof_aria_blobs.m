% Export configuration blobs for Aria

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2024, Intel Corporation.

function sof_aria_blobs()

	% Common definitions
	sof_tools = '../../../../tools';
	sof_tplg = fullfile(sof_tools, 'topology');
	fn.tpath =  fullfile(sof_tplg, 'topology2/include/components/aria');

	% Set the parameters here
	sof_tools = '../../../../tools';
	sof_tplg = fullfile(sof_tools, 'topology');
	sof_tplg_aria = fullfile(sof_tplg, 'topology2/include/components/aria');

	sof_aria_paths(true);

	blob8 = sof_aria_build_blob(0);
	tplg2_fn = sprintf("%s/passthrough.conf", sof_tplg_aria);
	check_create_dir(tplg2_fn);
	tplg2_write(tplg2_fn, blob8, "aria_config", ...
		    "Exported with script sof_aria_blobs.m" , ...
		    "cd tools/tune/aria; octave sof_aria_blobs.m");

	for param = 1:3
		blob8 = sof_aria_build_blob(param);
		tplg2_fn = sprintf("%s/param_%d.conf", sof_tplg_aria, param);
		tplg2_write(tplg2_fn, blob8, "aria_config", ...
			    "Exported with script sof_aria_blobs.m" , ...
			    "cd tools/tune/aria; octave sof_aria_blobs.m");
	end

	sof_aria_paths(false);
end

function sof_aria_paths(enable)

	common = '../../../../tools/tune/common';
	if enable
		addpath(common);
	else
		rmpath(common);
	end
end

function blob8 = sof_aria_build_blob(param_values)

	blob_type = 0;
	blob_param_id = 1; % ARIA_SET_ATTENUATION
	data_length = length(param_values);
	data_size = 4 * data_length;
	ipc_ver = 4;
	[abi_bytes, abi_size] = get_abi(data_size, ipc_ver, blob_type, blob_param_id);
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
