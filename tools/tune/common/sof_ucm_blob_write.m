function sof_ucm_blob_write(fn, blob8)

% Export blob to UCM2 cset-tlv binary format
%
% sof_ucm_blob_write(fn, blob)
%
% Input parameters
%  fn - Filename for the blob
%  blob - Vector of data with uint8 type
%

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2024, Intel Corporation. All rights reserved.

% Export for UCM cset-tlv with additional 8 bytes header
SOF_CTRL_CMD_BINARY = 3;
nh = 8;
nb = length(blob8);
ublob8 = zeros(nb + nh, 1, 'uint8');
ublob8(1:4) = w32b(SOF_CTRL_CMD_BINARY);
ublob8(5:8) = w32b(nb);
ublob8(9:end) = blob8;

%% Write blob
check_create_dir(fn);
fh = fopen(fn, 'wb');
if fh < 0
	fprintf(1, 'Error: Could not open file %s\n', fn);
	error("Failed.");
end

fwrite(fh, ublob8, 'uint8');
fclose(fh);

%% Print as 8 bit hex
nb = length(ublob8);
nl = ceil(nb/16);
for i = 1:nl
	m = min(16, nb-(i-1)*16);
	for j = 1:m
		fprintf(1, "%02x ", ublob8((i-1)*16 + j));
	end
	fprintf(1, "\n");
end

fprintf(1, "\n");
end

function bytes = w32b(word)
sh = [0 -8 -16 -24];
bytes = uint8(zeros(1,4));
bytes(1) = bitand(bitshift(word, sh(1)), 255);
bytes(2) = bitand(bitshift(word, sh(2)), 255);
bytes(3) = bitand(bitshift(word, sh(3)), 255);
bytes(4) = bitand(bitshift(word, sh(4)), 255);
end
