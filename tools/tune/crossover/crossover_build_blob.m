function blob8 = crossover_build_blob(blob_struct, endian)

%% Settings
bits_R = 32; %Q2.30
qy_R = 30;

if nargin < 2
        endian = 'little'
endif

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
[abi_bytes, abi_size] = crossover_get_abi(data_size);

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

endfunction

function bytes = word2byte(word, sh)
bytes = uint8(zeros(1,4));
bytes(1) = bitand(bitshift(word, sh(1)), 255);
bytes(2) = bitand(bitshift(word, sh(2)), 255);
bytes(3) = bitand(bitshift(word, sh(3)), 255);
bytes(4) = bitand(bitshift(word, sh(4)), 255);
end

function [bytes, nbytes] = crossover_get_abi(setsize)

%% Return current SOF ABI header
%% Use sof-ctl to write ABI header into a file
abifn = 'crossover_get_abi.bin';
cmd = sprintf('sof-ctl -g %d -b -o %s', setsize, abifn);
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
