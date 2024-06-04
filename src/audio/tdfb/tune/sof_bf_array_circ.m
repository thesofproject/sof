% bf = sof_bf_array_circ(bf)
%
% Inputs
% bf.mic_n ... number of microphones
% bf.mic_r ... radius of circular array [m]
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

function bf = sof_bf_array_circ(bf)

bf.mic_angle = (0:bf.mic_n-1)*360/bf.mic_n; % Mic 1 at 0 deg
idx = find(bf.mic_angle > 180); % wrap > 180 deg to -180 .. 0
bf.mic_angle(idx) = bf.mic_angle(idx)-360;
bf.mic_x = bf.mic_r*cosd(bf.mic_angle);
bf.mic_y = bf.mic_r*sind(bf.mic_angle);
bf.mic_z = zeros(1,bf.mic_n);
bf.mic_d = sqrt((bf.mic_x(1)-bf.mic_x(2))^2+(bf.mic_y(1)-bf.mic_y(2))^2);

end
