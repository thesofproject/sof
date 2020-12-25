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
function x = data_initialization()
x =  fi((-31.9:0.1:31.9),1,32,26); %Q6.26
