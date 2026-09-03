% bf = sof_bf_filenames_helper(bf, id)
%
% Automatically defines output files names based on array geometry
% and steer angle.

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2020, Intel Corporation. All rights reserved.
%
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

function bf = sof_bf_filenames_helper(bf, id)

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

if length(bf.steer_az) ~= length(bf.steer_el)
	error('The steer_az and steer_el lengths need to be equal.');
end

[az_str_pm] = angles_to_str(bf.steer_az);
[el_str_pm] = angles_to_str(bf.steer_el);

if strcmp(bf.array, 'line')
	el_str_show = '';
else
	el_str_show = sprintf('_el%s', el_str_pm);
end

if nargin > 1
    idstr = sprintf('%s_az%s%s_%dkhz', ...
                id, az_str_pm, el_str_show, round(bf.fs/1e3));
else
    idstr = sprintf('%s%s_%smm_az%s%s_%dkhz', ...
                bf.array, mic_n_str, mic_d_str, ...
                az_str_pm, el_str_show, round(bf.fs/1e3));
end

% Contain multiple (az, el) angles
bf.sofctl3_fn = fullfile(bf.sofctl3_path, sprintf('coef_%s.txt', idstr));
bf.tplg1_fn = fullfile(bf.tplg1_path, sprintf('coef_%s.m4', idstr));
bf.sofctl4_fn = fullfile(bf.sofctl4_path, sprintf('%s.txt', idstr));
bf.ucmbin3_fn = fullfile(bf.sofctl3_path, sprintf('%s.bin', idstr));
bf.ucmbin4_fn = fullfile(bf.sofctl4_path, sprintf('%s.bin', idstr));
bf.tplg2_fn = fullfile(bf.tplg2_path, sprintf('%s.conf', idstr));


for n = 1:length(bf.steer_az)

	az = bf.steer_az(n);
	el = bf.steer_el(n);

    if nargin > 1
        bf.array_id{n} = sprintf('%s (%d, %d) deg', ...
                      id, az, el);
        idstr = sprintf('%s_az%sel%sdeg_%dkhz', ...
                id, numpm(az), numpm(el), round(bf.fs/1e3));
    else
        bf.array_id{n} = sprintf('%s %s mic %s mm (%d, %d) deg', ...
                      bf.array, mic_n_str, mic_d_str, ...
                      az, el);
        idstr = sprintf('%s%s_%smm_az%sel%sdeg_%dkhz', ...
                bf.array, mic_n_str, mic_d_str, ...
                numpm(az), numpm(el), round(bf.fs/1e3));
    end

	% Single (az, el) value per file
	bf.mat_fn{n} = fullfile(bf.data_path, sprintf('tdfb_coef_%s.mat', idstr));
	bf.noiserot_fn{n} = fullfile(bf.data_path, sprintf('simcap_noiserot_%s.raw', idstr));
	bf.sinerot_fn{n} = fullfile(bf.data_path, sprintf('simcap_sinerot_%s.raw', idstr));
	bf.diffuse_fn{n} = fullfile(bf.data_path, sprintf('simcap_diffuse_%s.raw', idstr));
	bf.random_fn{n} = fullfile(bf.data_path, sprintf('simcap_random_%s.raw', idstr));

end

end

function nstr = numpm(n)
	if n < 0
		nstr = sprintf('m%d', -round(n));
	else
		nstr = sprintf('%d', round(n));
	end
end

function a_str_pm = angles_to_str(a)
	if length(a) > 1
		a_min = min(round(a));
		a_max = max(round(a));
		n = length(a);
		a_str_pm = sprintf('%s_%s_%d', numpm(a_min), numpm(a_max), n);
	else
		a_str_pm = numpm(a);
	end
end
