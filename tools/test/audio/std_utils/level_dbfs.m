function dbfs = level_dbfs(x)

%% dbfs = level_dbfs(x)
%
% Input
% x - signal
%
% Output
% dbfs - signal level in dBFS
%

% SPDX-License-Identifier: BSD-3-Clause
% Copyright(c) 2017 Intel Corporation. All rights reserved.
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

%% Reference AES 17 3.12.3
dbfs = 20*log10(rms(x) * sqrt(2));

end
