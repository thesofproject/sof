% bf = sof_bf_array_rot(bf)
%
% Inputs
% bf.array_angle ... three element vector for x, y, z rotation [degrees]
% bf.mic_x ......... x coordinates of microphones in [m]
% bf.mic_y ......... y coordinates of microphones in [m]
% bf.mic_z ......... z coordinates of microphones in [m]
%
% Outputs
% bf.mic_x
% bf.mic_y
% bf.mic_z

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2020, Intel Corporation. All rights reserved.
%
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

function bf = sof_bf_array_rot(bf)

% Equations reference
% https://en.wikipedia.org/wiki/Rotation_matrix#Basic_rotations

% Rotate around X-axis
%
% | x' |   | 1    0      0   | | x |
% | y' | = | 0   cosa  -sina | | y |
% | z' |   | 0   sina   cosa | | z |
a = bf.array_angle(1) * pi/180;
y = bf.mic_y;
z = bf.mic_z;
bf.mic_y = cos(a) * y - sin(a) * z;
bf.mic_z = sin(a) * y + cos(a) * z;

% Rotate around Y-axis
%
% | x' |   |  cosa   0  sina  | | x |
% | y' | = |    0    1    0   | | y |
% | z' |   | -sina   0  cosa  | | z |
a = bf.array_angle(2) * pi/180;
x = bf.mic_x;
z = bf.mic_z;
bf.mic_x =  cos(a) * x + sin(a) * z;
bf.mic_z = -sin(a) * x + cos(a) * z;

% Rotate around Z-axis
%
% | x' |   | cosa  -sina  0 | | x |
% | y' | = | sina   cosa  0 | | y |
% | z' |   |   0      0   1 | | z |
a = bf.array_angle(3) * pi/180;
x = bf.mic_x;
y = bf.mic_y;
bf.mic_x = cos(a) * x - sin(a) * y;
bf.mic_y = sin(a) * x + cos(a) * y;

end
