function tplg_write(fn, blob8, name, comment)

if nargin < 4
	comment = 'Exported Control Bytes';
end

%% Pad blob length to multiple of four bytes
n_orig = length(blob8);
n_new = ceil(n_orig/4)*4;
blob8_new = zeros(1, n_new);
blob8_new(1:n_orig) = blob8;

%% Write blob
fh = fopen(fn, 'w');
nl = 8;
fprintf(fh, '# %s %s\n', comment, date());
fprintf(fh, 'CONTROLBYTES_PRIV(%s_priv,\n', name);
fprintf(fh, '`       bytes "');
for i = 1:nl:n_new
	if i > 1
		fprintf(fh, '`       ');
	end
	for j = 0:nl-1
		n = i + j;
		if n < n_new
			fprintf(fh, '0x%02x,', blob8_new(n));
		end
		if n == n_new
			fprintf(fh, '0x%02x"', blob8_new(n));
		end
	end
	fprintf(fh, '''\n');
end
fprintf(fh, ')\n');
fclose(fh);

end
