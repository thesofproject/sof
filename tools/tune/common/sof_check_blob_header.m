% sof_check_blob_header(blob)
%
% Check for correct header in bytes data. The function
% errors if header is not correct.
%
% Input
%	blob - int8 type numbers data
%
% Output
%	<none>

function sof_check_blob_header(blob8)

% Correct size in header?
blob_bytes = length(blob8);
header_bytes = b2w(blob8(5:8));
if blob_bytes ~= header_bytes + 8
	fprintf(1, "Error: blob header size %d does not math blob size %d\n", header_bytes, blob_bytes);
	fprintf(1, "Is installed sof-ctl up-to-date?\n");
	error("Failed.");
end

% Correct command?
SOF_CTRL_CMD_BINARY = 3;
value = b2w(blob8(1:4));
if value ~= SOF_CTRL_CMD_BINARY
	fprintf(1, "Error: blob control command is not set to SOF_CTRL_CMD_BINARY.\n");
	fprintf(1, "Is installed sof-ctl up-to-date?\n");
	error("Failed.");
end

end

function word =b2w(bytes)
	tmp = int32(bytes);
	word = tmp(1) + bitshift(tmp(2), 8) + bitshift(tmp(3), 16) + bitshift(tmp(4), 24);
end
