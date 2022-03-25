function eq_tplg2_write(fn, blob8, name, comment)

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2018-2020, Intel Corporation. All rights reserved.
%
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

if nargin < 4
	comment = 'Exported EQ';
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
fprintf(fh, 'Object.Base.data."%s" {\n', name);
fprintf(fh, '\tbytes  "');
for i = 1:nl:n_new
	if i > 1
		fprintf(fh, '\t\t');
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
	fprintf(fh, '\n');
end
fprintf(fh, '}\n');
fclose(fh);

end
