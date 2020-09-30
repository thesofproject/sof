function dbfs = level_dbfs(x)

%% dbfs = level_dbfs(x)
%
% Input
% x - signal
%
% Output
% dbfs - sigmal level in dBFS
%

% SPDX-License-Identifier: BSD-3-Clause
% Copyright(c) 2017 Intel Corporation. All rights reserved.
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

%% Reference AES 17 3.12.3
level_ms = mean(x.^2);
dbfs = 10*log10(level_ms + 1e-20) + 20*log10(sqrt(2));

end
