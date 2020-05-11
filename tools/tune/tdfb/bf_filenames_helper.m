% bf = bf_filenames_helper(bf, tplg_path, ctl_path, data_path)
%
% Automatically defines output files names based on array geometry
% and steer angle.

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2020, Intel Corporation. All rights reserved.
%
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

function bf = bf_filenames_helper(bf, tplg_path, ctl_path, data_path)


bf.array_id = sprintf('%s %d mic %d mm (%d, %d) deg', ...
		      bf.array, bf.mic_n, bf.mic_d * 1e3, ...
		      bf.steer_az, bf.steer_el);

idstr = sprintf('%s%d_%dmm_az%sel%sdeg_%dkhz', ...
		bf.array, bf.mic_n, round(bf.mic_d * 1e3), ...
		numpm(bf.steer_az), numpm(bf.steer_el), round(bf.fs/1e3));

bf.sofctl_fn = fullfile(ctl_path, sprintf('coef_%s.txt', idstr));
bf.tplg_fn = fullfile(tplg_path, sprintf('coef_%s.m4', idstr));
bf.mat_fn = fullfile(data_path, sprintf('tdfb_coef_%s.mat', idstr));
bf.sinerot_fn = fullfile(data_path, sprintf('simcap_sinerot_%s.raw', idstr));
bf.diffuse_fn = fullfile(data_path, sprintf('simcap_diffuse_%s.raw', idstr));
bf.random_fn = fullfile(data_path, sprintf('simcap_random_%s.raw', idstr));

end

function nstr = numpm(n)
	if n < 0
		nstr = sprintf('m%d', -round(n));
	else
		nstr = sprintf('%d', round(n));
	end
end
