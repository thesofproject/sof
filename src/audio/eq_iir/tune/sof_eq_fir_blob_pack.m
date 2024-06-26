function blob8 = sof_eq_fir_blob_pack(bs, ipc_ver, endian)

%% Pack equalizer struct to bytes
%
% blob8 = sof_eq_fir_blob_pack(bs, ipc_ver, endian)
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

%% Endianness of blob
switch lower(endian)
        case 'little'
                sh16 = [0 -8];
                sh32 = [0 -8 -16 -24];
        case 'big'
                sh16 = [-8 0];
                sh32 = [-24 -16 -8 0];
        otherwise
                error('Unknown endianness');
end

%% Channels count must be even
if mod(bs.channels_in_config, 2) > 0
	error("Channels # must be even");
end

%% Channels count and assign vector length must be the same
if bs.channels_in_config ~= length( bs.assign_response)
	error("Channels # and response assign length must match");
end

%% Coefficients vector length must be multiple of 4
len = length(bs.all_coefficients);
len_no_header = len - 10 * bs.number_of_responses_defined;
if mod(len_no_header, 4) > 0
	error("Coefficient data vector length must be multiple of 4");
end

%% Header format is
%	uint32_t size;
%	uint16_t channels_in_config;
%	uint16_t number_of_responses;
%	uint32_t reserved[4];
%	int16_t data[];

%% Pack as 16 bits
nh16 = 12+bs.channels_in_config;
h16 = zeros(1, nh16, 'int16');
nc16 = length(bs.all_coefficients);
nb16 = ceil((nh16+nc16)/2)*2;
h16(1) = 2 * nb16;
h16(2) = 0;
h16(3) = bs.channels_in_config;
h16(4) = bs.number_of_responses_defined;
h16(5) = 0;
h16(6) = 0;
h16(7) = 0;
h16(8) = 0;
h16(9) = 0;
h16(10) = 0;
h16(11) = 0;
h16(12) = 0;
for i=1:bs.channels_in_config
        h16(12+i) = bs.assign_response(i);
end

%% Merge header and coefficients, make even number of int16 to make it
%  multiple of int32
blob16 = zeros(1,nb16, 'int16');
blob16(1:nh16) = h16;
blob16(nh16+1:nh16+nc16) = int16(bs.all_coefficients);

%% Pack as 8 bits
nbytes_data = nb16 * 2;

%% Get ABI information
[abi_bytes, nbytes_abi] = sof_eq_get_abi(nbytes_data, ipc_ver);

%% Initialize uint8 array with correct size
nbytes = nbytes_abi + nbytes_data;
blob8 = zeros(1, nbytes, 'uint8');

%% Inset ABI header
blob8(1:nbytes_abi) = abi_bytes;
j = nbytes_abi + 1;

%% Component data
for i = 1:length(blob16)
        blob8(j:j+1) = w16b(blob16(i), sh16);
        j = j+2;
end

%% Done
fprintf('Blob size is %d bytes.\n', nbytes);

end

function bytes = w16b(word, sh)
bytes = uint8(zeros(1,2));
bytes(1) = bitand(bitshift(word, sh(1)), 255);
bytes(2) = bitand(bitshift(word, sh(2)), 255);
end

function bytes = w32b(word, sh)
bytes = uint8(zeros(1,4));
bytes(1) = bitand(bitshift(word, sh(1)), 255);
bytes(2) = bitand(bitshift(word, sh(2)), 255);
bytes(3) = bitand(bitshift(word, sh(3)), 255);
bytes(4) = bitand(bitshift(word, sh(4)), 255);
end
