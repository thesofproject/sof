function eq_tplg_write(fn, blob8, priv, comment)

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2018-2020, Intel Corporation. All rights reserved.
%
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

if nargin < 4
	comment = 'Exported EQ';
end

%% Check that blob length is multiple of 32 bits
n_blob = length(blob8);
n_test = ceil(n_blob/4)*4;
if (n_blob ~= n_test)
	fprintf(1, 'Error: ´Blob length %d is not multiple of 32 bits\n', ...
		n_blob);
	error('Failed.');
end

%% Write blob
fh = fopen(fn, 'w');
nl = 8;
fprintf(fh, '# %s %s\n', comment, date());
fprintf(fh, 'CONTROLBYTES_PRIV(%s,\n', priv);
fprintf(fh, '`       bytes "');
for i = 1:nl:n_blob
	if i > 1
		fprintf(fh, '`       ');
	end
	for j = 0:nl-1
		n = i + j;
		if n < n_blob
			fprintf(fh, '0x%02x,', blob8(n));
		end
		if n == n_blob
			fprintf(fh, '0x%02x"', blob8(n));
		end
	end
	fprintf(fh, '''\n');
end
fprintf(fh, ')\n');
fclose(fh);
fprintf('Blob size %d was written to file %s\n', n_blob, fn);

end
