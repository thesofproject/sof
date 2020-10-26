% bf = bf_array_lshape(bf)
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

function bf = bf_array_lshape(bf)

bf.mic_x = [];
bf.mic_y = [];

bf.mic_n = sum(bf.mic_nxy) -1;
bf.mic_z = zeros(1, bf.mic_n);
n = 1;
for x = 0:(bf.mic_nxy(1) -1)
	bf.mic_x(n) = 0;
	bf.mic_y(n) = -x * bf.mic_dxy(1);
	n = n + 1;
end

for y = 1:(bf.mic_nxy(2) -1)
	bf.mic_x(n) = y * bf.mic_dxy(2);
	bf.mic_y(n) = 0;
	n = n + 1;
end

bf.mic_x = bf.mic_x - mean(bf.mic_x);
bf.mic_y = bf.mic_y - mean(bf.mic_y);
bf.mic_z = bf.mic_z - mean(bf.mic_z);

bf.mic_d = max(bf.mic_dxy);

end
