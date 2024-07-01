% bf = sof_bf_array_line(bf)
%
% Inputs
% bf.mic_n ... number of microphones
% bf.mic_d ... distance between microphones [m]
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

function bf = sof_bf_array_line(bf)

bf.mic_y = linspace(0, -(bf.mic_n-1) * bf.mic_d, bf.mic_n) ...
	+ (bf.mic_n-1) * bf.mic_d / 2;
bf.mic_x = zeros(1, bf.mic_n);
bf.mic_z = zeros(1, bf.mic_n);

end
