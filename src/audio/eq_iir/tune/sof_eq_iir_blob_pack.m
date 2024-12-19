function blob8 = sof_eq_iir_blob_pack(bs, ipc_ver, endian)

%% Pack equalizer struct to bytes
%
% blob8 = sof_eq_iir_blob_pack(bs, endian)
% bs - blob struct
% ipc_ver - optional, use 3 or 4. Default is 3.
% endian - optional, use 'little' or 'big'. Defaults to little.
%

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright(c) 2016 Intel Corporation. All rights reserved.
%
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

if nargin < 2
	ipc_ver = 3;
end

if nargin < 3
	endian = 'little';
end

%% Channels count and assign vector lengths must be the same
if bs.channels_in_config ~= length( bs.assign_response)
	error("Channels # and response assign length must match");
end

%% Shift values for little/big endian
switch lower(endian)
        case 'little'
                sh = [0 -8 -16 -24];
        case 'big'
                sh = [-24 -16 -8 0];
        otherwise
                error('Unknown endiannes');
end

%% Pack as 8 bits, header
nbytes_head = (7+bs.channels_in_config)*4;
nbytes_coef = length(bs.all_coefficients)*4;
nbytes_data = nbytes_head + nbytes_coef;

%% Get ABI information
[abi_bytes, nbytes_abi] = sof_get_abi(nbytes_data, ipc_ver);

%% Initialize correct size uint8 array
nbytes = nbytes_abi + nbytes_data;
blob8 = uint8(zeros(1,nbytes));

%% Insert ABI header
blob8(1:nbytes_abi) = abi_bytes;
j = nbytes_abi + 1;

%% Component data
blob8(j:j+3) = w2b(nbytes_data, sh); j=j+4;
blob8(j:j+3) = w2b(bs.channels_in_config, sh); j=j+4;
blob8(j:j+3) = w2b(bs.number_of_responses_defined, sh); j=j+4;
blob8(j:j+3) = w2b(0, sh);j=j+4; % Reserved
blob8(j:j+3) = w2b(0, sh);j=j+4; % Reserved
blob8(j:j+3) = w2b(0, sh);j=j+4; % Reserved
blob8(j:j+3) = w2b(0, sh);j=j+4; % Reserved

for i=1:bs.channels_in_config
        blob8(j:j+3) = w2b(int32(bs.assign_response(i)), sh);
	j=j+4;
end

%% Pack coefficients
for i=1:length(bs.all_coefficients)
        blob8(j:j+3) = w2b(int32(bs.all_coefficients(i)), sh);
	j=j+4;
end
fprintf('Blob size is %d bytes.\n', nbytes);

end

function bytes = w2b(word, sh)
bytes = uint8(zeros(1,4));
bytes(1) = bitand(bitshift(word, sh(1)), 255);
bytes(2) = bitand(bitshift(word, sh(2)), 255);
bytes(3) = bitand(bitshift(word, sh(3)), 255);
bytes(4) = bitand(bitshift(word, sh(4)), 255);
end
