%  SPDX-License-Identifier: BSD-3-Clause
%
%  Copyright(c) 2020 Intel Corporation. All rights reserved.
%
%  Author: Shriram Shastry <malladi.sastry@linux.intel.com>
%---------------------------------------------------
%---------------------------------------
%   History
%---------------------------------------
%   2020/12/24 Sriram Shastry       - initial version
%
function[ydB]= drc_mag2db(x)
x(x<0) = NaN;
qsb  = fixed.Quantizer(numerictype(1,32,26)); % Quantizer, Q11.22
ydB  = quantize(qsb,20*fi(sign(x)).*fi(log(double(x)),1,32,26)/fi(log(10),1,32,26));%Q11.22
