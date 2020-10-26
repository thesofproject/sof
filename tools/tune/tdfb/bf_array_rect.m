% bf = bf_array_rect(bf)
%
% Inputs
% bf.mic_nxy ... vector of two with number of microphones along x and y
% bf.mic_rxy ... vector of two with distance along x and y [m]
%
% Outputs
% bf.mic_x ... x coordinates [m]
% bf.mic_y ... y coordinates [m]
% bf.mic_z ... z coordinates [m]

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2020, Intel Corporation. All rights reserved.
%
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

function bf = bf_array_rect(bf)

bf.mic_x = [];
bf.mic_y = [];

bf.mic_n = prod(bf.mic_nxy);
bf.mic_z = zeros(1, bf.mic_n);
for y = 1:bf.mic_nxy(2)
	for x = 1:bf.mic_nxy(1)
		n = x + bf.mic_nxy(1) * (y - 1);
		bf.mic_y(n) = -x * bf.mic_dxy(1);
		bf.mic_x(n) = y * bf.mic_dxy(2);
	end
end

bf.mic_x = bf.mic_x - mean(bf.mic_x);
bf.mic_y = bf.mic_y - mean(bf.mic_y);
bf.mic_z = bf.mic_z - mean(bf.mic_z);
bf.mic_d = max(bf.mic_dxy);

end
