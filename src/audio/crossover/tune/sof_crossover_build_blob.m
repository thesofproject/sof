function blob8 = sof_crossover_build_blob(blob_struct, endian, ipc_ver)

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

%% Build Blob
% refer to sof/src/include/user/crossover.h for the config struct.
data_size = 4 * (2 + 4 + 4 + numel(blob_struct.all_coef));
[abi_bytes, abi_size] = sof_get_abi(data_size, ipc_ver);

blob_size = data_size + abi_size;
blob8 = uint8(zeros(1, blob_size));

% Pack Blob data
% Insert ABI Header
blob8(1:abi_size) = abi_bytes;
j = abi_size + 1;

% Insert Data
blob8(j:j+3) = word2byte(data_size, sh); j=j+4;
blob8(j:j+3) = word2byte(blob_struct.num_sinks, sh); j=j+4;
blob8(j:j+3) = word2byte(0, sh); j=j+4; % Reserved
blob8(j:j+3) = word2byte(0, sh); j=j+4; % Reserved
blob8(j:j+3) = word2byte(0, sh); j=j+4; % Reserved
blob8(j:j+3) = word2byte(0, sh); j=j+4; % Reserved

for i=1:4
	blob8(j:j+3) = word2byte(blob_struct.assign_sinks(i), sh);
	j=j+4;
end

for i=1:length(blob_struct.all_coef)
	blob8(j:j+3) = word2byte(blob_struct.all_coef(i), sh);
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
