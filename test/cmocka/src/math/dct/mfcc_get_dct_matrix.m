% dct_matrix = mfcc_get_dct_matrix(num_ceps, num_mels, type)
%
% Input
%   num_ceps - number of cepstral coefficients
%   num_mels - number of Mel band energies
%   type - DCT type, currently only 2 for DCT-II is supported
%
% Output
%   dct matrix coefficients

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2022, Intel Corporation. All rights reserved.

% Ref: pytorch kaldi.py _get_dct_matrix
function dct_matrix = mfcc_get_dct_matrix(num_ceps, num_mels, type)

dct_matrix = create_dct(num_ceps, num_mels, type, 'ortho');

% TODO: Check if code below is needed
% make first cepstral to be weighted sum of factor sqrt(1 / num_mels)
%dct_matrix(:,1) = sqrt(1 / num_mels);
%dct_matrix = dct_matrix(:, 1:num_ceps);

end


% Ref pytorch functional.py create_dct
function dct_matrix = create_dct(n_mfcc, n_mels, type, norm)

debug = 0;
n = (0:(n_mels - 1))'; % Size n_mels x 1
k = 0:(n_mfcc - 1); % Size 1 x n_mfcc
switch type
	case 2,
		arg = pi / n_mels * (n + 0.5) * k;
		dct_matrix = cos(arg);
		if debug
			fprintf(1, 'Max arg = %f\n', max(max(arg)));
		end
	case 3,
		error('not implemented'); % FIXME
	otherwise
		error('Unknown DCT type');
end

switch lower(norm)
	case 'ortho',
		dct_matrix(:,1) = dct_matrix(:,1) * 1 / sqrt(2);
		dct_matrix = dct_matrix * sqrt(2 / n_mels);
	case 'none'
		dct_matrix = dct_matrix * 2;
	otherwise
		error('Unknown norm');
end

end
