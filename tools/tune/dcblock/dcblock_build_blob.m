function blob8 = dcblock_build_blob(R_coeffs, endian, ipc_ver)

%% Settings
qy_R = 30;

if nargin < 2
        endian = 'little';
end

if nargin < 3
        ipc_ver = 3;
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

%% Convert R_coeffs from float to Q2.30 integers.
num_of_coeffs = length(R_coeffs);
R_coeffs = int32(R_coeffs * bitshift(1, qy_R) + 0.5);

%% Build Blob
data_size = (num_of_coeffs)*4;
[abi_bytes, abi_size] = get_abi(data_size, ipc_ver);

blob_size = data_size + abi_size;
blob8 = uint8(zeros(1, blob_size));

% Pack Blob data
% Insert ABI Header
blob8(1:abi_size) = abi_bytes;
j = abi_size + 1;

% Insert Data
for i=1:num_of_coeffs
        blob8(j:j+3) = word2byte(R_coeffs(i), sh);
	j=j+4;
end

end

function bytes = word2byte(word, sh)
bytes = uint8(zeros(1,4));
bytes(1) = bitand(bitshift(word, sh(1)), 255);
bytes(2) = bitand(bitshift(word, sh(2)), 255);
bytes(3) = bitand(bitshift(word, sh(3)), 255);
bytes(4) = bitand(bitshift(word, sh(4)), 255);
end
