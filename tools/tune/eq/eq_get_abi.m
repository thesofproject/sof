function [bytes, nbytes] = eq_get_abi(setsize, ipc_ver, type, param_id)

%% Return current SOF ABI header
%
% [bytes, nbytes] = eq_get_abi(setsize, ipc_ver, type, param_id)
% Input
%	setsize - size of the payload blob
%	ipc_ver - 3 or 4, defaults to 3
%	type - for IPC3, normally 0, defaults to 0
%	param_id - for IPC4, 0 - 255, defaults to 0
%
% Output
%	bytes - returned uint8 type array
%	nbytes - length of the array

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright(c) 2018 Intel Corporation. All rights reserved.
%
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

if nargin < 2
	ipc_ver = 3;
end
if nargin < 3
	type = 0;
end
if nargin < 4
	param_id = 0;
end

abifn = 'eq_get_abi.bin';
switch ipc_ver
	case 3
		%% Use sof-ctl to write ABI header into a file
		cmd = sprintf('sof-ctl -g %d -t %d -b -o %s', setsize, type, abifn);
		[bytes, nbytes] = get_abi_with_sofctl(cmd, abifn);
	case 4
		cmd = sprintf('sof-ctl -i 4 -g %d -p %d -b -o %s', setsize, param_id, abifn);
		[bytes, nbytes] = get_abi_with_sofctl(cmd, abifn);
	otherwise
		fprintf(1, 'Illegal ipc_ver %d\n', ipc_ver);
		error('Failed.');
end

end

function [bytes, nbytes] = get_abi_with_sofctl(cmd, abifn)

[ret, ~] = system(cmd);
if ret
	error('Failed to run sof-ctl. Is it installed?');
end
fh = fopen(abifn, 'r');
if fh < 0
	error('Failed to read abi header produced by sof-ctl');
end
[bytes, nbytes] = fread(fh, inf, 'uint8');
fclose(fh);
delete(abifn);

end

function bytes = w32b(word)

sh = [0 -8 -16 -24];
bytes = uint8(zeros(1,4));
bytes(1) = bitand(bitshift(word, sh(1)), 255);
bytes(2) = bitand(bitshift(word, sh(2)), 255);
bytes(3) = bitand(bitshift(word, sh(3)), 255);
bytes(4) = bitand(bitshift(word, sh(4)), 255);

end
