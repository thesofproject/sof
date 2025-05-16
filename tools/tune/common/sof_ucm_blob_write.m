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

%% Check that blob header is sane
sof_check_blob_header(blob8);

%% Write blob
sof_check_create_dir(fn);
fh = fopen(fn, 'wb');
if fh < 0
	fprintf(1, 'Error: Could not open file %s\n', fn);
	error("Failed.");
end

fwrite(fh, blob8, 'uint8');
fclose(fh);

%% Print as 8 bit hex
nb = length(blob8);
nl = ceil(nb/16);
for i = 1:nl
	m = min(16, nb-(i-1)*16);
	for j = 1:m
		fprintf(1, "%02x ", blob8((i-1)*16 + j));
	end
	fprintf(1, "\n");
end

fprintf(1, "\n");
end
