function blob8 = multiband_drc_build_blob(num_bands, enable_emp_deemp,
					  emp_coefs, deemp_coefs,
					  crossover_coefs, drc_coefs,
					  endian);

if nargin < 7
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
% refer to sof/src/include/user/multiband_drc.h for the config struct.
emp_data_num = numel(emp_coefs);
deemp_data_num = numel(deemp_coefs);
crossover_data_num = numel(crossover_coefs);
drc_data_num = numel(fieldnames(drc_coefs(1))) * length(drc_coefs);
data_size = 4 * (1 + 1 + 1 + 8 + emp_data_num + deemp_data_num + crossover_data_num + drc_data_num);
[abi_bytes, abi_size] = multiband_drc_get_abi(data_size);

blob_size = data_size + abi_size;
blob8 = uint8(zeros(1, blob_size));

% Pack Blob data
% Insert ABI Header
blob8(1:abi_size) = abi_bytes;
j = abi_size + 1;

% Insert Data
blob8(j:j+3) = word2byte(data_size, sh); j=j+4; % size
blob8(j:j+3) = word2byte(num_bands, sh); j=j+4; % num_bands
blob8(j:j+3) = word2byte(enable_emp_deemp, sh); j=j+4; % enable_emp_deemp
for i = 1:8
	blob8(j:j+3) = word2byte(0, sh); j=j+4; % reserved
end
for i = 1:length(emp_coefs)
	blob8(j:j+3) = word2byte(emp_coefs(i), sh); j=j+4; % emp_coef
end
for i = 1:length(deemp_coefs)
	blob8(j:j+3) = word2byte(deemp_coefs(i), sh); j=j+4; % deemp_coef
end
for i = 1:length(crossover_coefs)
	blob8(j:j+3) = word2byte(crossover_coefs(i), sh); j=j+4; % crossover_coef
end
for i = 1:length(drc_coefs) % drc_coef
	blob8(j:j+3) = word2byte(drc_coefs(i).enabled, sh); j=j+4;
	blob8(j:j+3) = word2byte(drc_coefs(i).db_threshold, sh); j=j+4;
	blob8(j:j+3) = word2byte(drc_coefs(i).db_knee, sh); j=j+4;
	blob8(j:j+3) = word2byte(drc_coefs(i).ratio, sh); j=j+4;
	blob8(j:j+3) = word2byte(drc_coefs(i).pre_delay_time, sh); j=j+4;
	blob8(j:j+3) = word2byte(drc_coefs(i).linear_threshold, sh); j=j+4;
	blob8(j:j+3) = word2byte(drc_coefs(i).slope, sh); j=j+4;
	blob8(j:j+3) = word2byte(drc_coefs(i).K, sh); j=j+4;
	blob8(j:j+3) = word2byte(drc_coefs(i).knee_alpha, sh); j=j+4;
	blob8(j:j+3) = word2byte(drc_coefs(i).knee_beta, sh); j=j+4;
	blob8(j:j+3) = word2byte(drc_coefs(i).knee_threshold, sh); j=j+4;
	blob8(j:j+3) = word2byte(drc_coefs(i).ratio_base, sh); j=j+4;
	blob8(j:j+3) = word2byte(drc_coefs(i).master_linear_gain, sh); j=j+4;
	blob8(j:j+3) = word2byte(drc_coefs(i).one_over_attack_frames, sh); j=j+4;
	blob8(j:j+3) = word2byte(drc_coefs(i).sat_release_frames_inv_neg, sh); j=j+4;
	blob8(j:j+3) = word2byte(drc_coefs(i).sat_release_rate_at_neg_two_db, sh); j=j+4;
	blob8(j:j+3) = word2byte(drc_coefs(i).kSpacingDb, sh); j=j+4;
	blob8(j:j+3) = word2byte(drc_coefs(i).kA, sh); j=j+4;
	blob8(j:j+3) = word2byte(drc_coefs(i).kB, sh); j=j+4;
	blob8(j:j+3) = word2byte(drc_coefs(i).kC, sh); j=j+4;
	blob8(j:j+3) = word2byte(drc_coefs(i).kD, sh); j=j+4;
	blob8(j:j+3) = word2byte(drc_coefs(i).kE, sh); j=j+4;
end

endfunction

function bytes = word2byte(word, sh)
bytes = uint8(zeros(1,4));
bytes(1) = bitand(bitshift(word, sh(1)), 255);
bytes(2) = bitand(bitshift(word, sh(2)), 255);
bytes(3) = bitand(bitshift(word, sh(3)), 255);
bytes(4) = bitand(bitshift(word, sh(4)), 255);
end

function [bytes, nbytes] = multiband_drc_get_abi(setsize)

%% Return current SOF ABI header
%% Use sof-ctl to write ABI header into a file
abifn = 'multiband_drc_get_abi.bin';
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
