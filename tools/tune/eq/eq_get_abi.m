function [bytes, nbytes] = eq_get_abi(setsize)

%% Return current SOF ABI header
%
% [bytes, nbytes] = eq_get_abi(setsize)
%

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright(c) 2018 Intel Corporation. All rights reserved.
%
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

%% Use sof-ctl to write ABI header into a file
abifn = 'eq_get_abi.bin';
cmd = sprintf('sof-ctl -g %d -b -o %s > /dev/null', setsize, abifn);
system(cmd);

%% Read file and delete it
fh = fopen(abifn, 'r');
if fh < 0
	error("Failed to get ABI header. Is sof-ctl installed?");
end
[bytes, nbytes] = fread(fh, inf, 'uint8');
fclose(fh);
delete(abifn);

end
