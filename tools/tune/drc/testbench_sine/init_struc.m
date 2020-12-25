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
function x = init_struc ()
x = fi((-1:0.1:1),1,32,30); %Q2.30
