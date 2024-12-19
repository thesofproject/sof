function sof_alsactl_write(fn, blob8)

%% Write blob
sof_check_create_dir(fn);
fh = fopen(fn, 'w');
if fh < 0
	fprintf(1, 'Error: Could not open file %s\n', fn);
	error("Failed.");
end

%% Pad blob length to multiple of four bytes
n_orig = length(blob8);
n_new = ceil(n_orig/4);
blob8_new = zeros(1, n_new*4);
blob8_new(1:n_orig) = blob8;

%% Convert to 32 bit
blob32 = zeros(1, n_new, 'uint32');
k = 2.^[0 8 16 24];
for i=1:n_new
	j = (i-1)*4;
	blob32(i) = blob8_new(j+1)*k(1) + blob8_new(j+2)*k(2) ...
		    +  blob8_new(j+3)*k(3) + blob8_new(j+4)*k(4);
end
for i=1:n_new-1
	fprintf(fh, '%ld,', blob32(i));
end
fprintf(fh, '%ld,\n', blob32(end));
fclose(fh);
end
