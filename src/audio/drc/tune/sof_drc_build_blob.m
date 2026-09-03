function blob8 = sof_drc_build_blob(blob_struct, endian, ipc_ver)

if nargin < 2
        endian = 'little'
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
% refer to sof/src/include/user/drc.h for the config struct.
num_coefs = length(fieldnames(blob_struct));
data_size = 4 * (1 + 4 + num_coefs);
[abi_bytes, abi_size] = sof_get_abi(data_size, ipc_ver);

blob_size = data_size + abi_size;
blob8 = uint8(zeros(1, blob_size));

% Pack Blob data
% Insert ABI Header
blob8(1:abi_size) = abi_bytes;
j = abi_size + 1;

% Insert Data
blob8(j:j+3) = word2byte(data_size, sh); j=j+4;
blob8(j:j+3) = word2byte(0, sh); j=j+4; % Reserved
blob8(j:j+3) = word2byte(0, sh); j=j+4; % Reserved
blob8(j:j+3) = word2byte(0, sh); j=j+4; % Reserved
blob8(j:j+3) = word2byte(0, sh); j=j+4; % Reserved

blob8(j:j+3) = word2byte(blob_struct.enabled, sh); j=j+4;
blob8(j:j+3) = word2byte(blob_struct.db_threshold, sh); j=j+4;
blob8(j:j+3) = word2byte(blob_struct.db_knee, sh); j=j+4;
blob8(j:j+3) = word2byte(blob_struct.ratio, sh); j=j+4;
blob8(j:j+3) = word2byte(blob_struct.pre_delay_time, sh); j=j+4;
blob8(j:j+3) = word2byte(blob_struct.linear_threshold, sh); j=j+4;
blob8(j:j+3) = word2byte(blob_struct.slope, sh); j=j+4;
blob8(j:j+3) = word2byte(blob_struct.K, sh); j=j+4;
blob8(j:j+3) = word2byte(blob_struct.knee_alpha, sh); j=j+4;
blob8(j:j+3) = word2byte(blob_struct.knee_beta, sh); j=j+4;
blob8(j:j+3) = word2byte(blob_struct.knee_threshold, sh); j=j+4;
blob8(j:j+3) = word2byte(blob_struct.ratio_base, sh); j=j+4;
blob8(j:j+3) = word2byte(blob_struct.master_linear_gain, sh); j=j+4;
blob8(j:j+3) = word2byte(blob_struct.one_over_attack_frames, sh); j=j+4;
blob8(j:j+3) = word2byte(blob_struct.sat_release_frames_inv_neg, sh); j=j+4;
blob8(j:j+3) = word2byte(blob_struct.sat_release_rate_at_neg_two_db, sh); j=j+4;
blob8(j:j+3) = word2byte(blob_struct.kSpacingDb, sh); j=j+4;
blob8(j:j+3) = word2byte(blob_struct.kA, sh); j=j+4;
blob8(j:j+3) = word2byte(blob_struct.kB, sh); j=j+4;
blob8(j:j+3) = word2byte(blob_struct.kC, sh); j=j+4;
blob8(j:j+3) = word2byte(blob_struct.kD, sh); j=j+4;
blob8(j:j+3) = word2byte(blob_struct.kE, sh); j=j+4;

end

function bytes = word2byte(word, sh)
bytes = uint8(zeros(1,4));
bytes(1) = bitand(bitshift(word, sh(1)), 255);
bytes(2) = bitand(bitshift(word, sh(2)), 255);
bytes(3) = bitand(bitshift(word, sh(3)), 255);
bytes(4) = bitand(bitshift(word, sh(4)), 255);
end
