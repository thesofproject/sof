function sof_blob_write(fn, blob8)


%% Write blob
check_create_dir(fn);
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
