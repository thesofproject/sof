% bf = bf_filenames_helper(bf)
%
% Automatically defines output files names based on array geometry
% and steer angle.

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2020, Intel Corporation. All rights reserved.
%
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

function bf = bf_filenames_helper(bf)


switch lower(bf.array)
	case {'rectangle' 'lshape'}
		mic_n_str = sprintf('%dx%d', bf.mic_nxy(1), bf.mic_nxy(2));
		dmm = round(1e3 * bf.mic_dxy);
		mic_d_str = sprintf('%dx%d', dmm(1), dmm(2));
	case 'circular'
		mic_n_str = sprintf('%d', bf.mic_n);
		mic_d_str = sprintf('%d', round(1e3 * bf.mic_r));
	case 'xyz'
		mic_n_str = sprintf('%d', length(bf.mic_x));
		mic_d_str = 'x';
	otherwise
		mic_n_str = sprintf('%d', bf.mic_n);
		mic_d_str = sprintf('%d', round(1e3 * bf.mic_d));
end

bf.array_id = sprintf('%s %s mic %s mm (%d, %d) deg', ...
		      bf.array, mic_n_str, mic_d_str, ...
		      bf.steer_az, bf.steer_el);

idstr = sprintf('%s%s_%smm_az%sel%sdeg_%dkhz', ...
		bf.array, mic_n_str, mic_d_str, ...
		numpm(bf.steer_az), numpm(bf.steer_el), round(bf.fs/1e3));

bf.sofctl_fn = fullfile(bf.sofctl_path, sprintf('coef_%s.txt', idstr));
bf.tplg_fn = fullfile(bf.tplg_path, sprintf('coef_%s.m4', idstr));
bf.mat_fn = fullfile(bf.data_path, sprintf('tdfb_coef_%s.mat', idstr));
bf.sinerot_fn = fullfile(bf.data_path, sprintf('simcap_sinerot_%s.raw', idstr));
bf.diffuse_fn = fullfile(bf.data_path, sprintf('simcap_diffuse_%s.raw', idstr));
bf.random_fn = fullfile(bf.data_path, sprintf('simcap_random_%s.raw', idstr));

end

function nstr = numpm(n)
	if n < 0
		nstr = sprintf('m%d', -round(n));
	else
		nstr = sprintf('%d', round(n));
	end
end
