function sof_tplg2_write(fn, blob8, component, comment, howto)

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2023, Intel Corporation. All rights reserved.
%
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

if nargin < 4
	comment = 'Exported Control Bytes';
end
if nargin < 5
	howto = [];
end

%% Check that blob header is sane
sof_check_blob_header(blob8);

%% Drop the 8 bytes TLV header from topology embedded blop
blob8 = blob8(9:end);

%% Check that blob length is multiple of 32 bits
n_blob = length(blob8);
n_test = ceil(n_blob/4)*4;
if (n_blob ~= n_test)
	fprintf(1, 'Error: ´Blob length %d is not multiple of 32 bits\n', ...
		n_blob);
	error('Failed.');
end

%% Write blob
sof_check_create_dir(fn);
fh = fopen(fn, 'w');
if fh < 0
	fprintf(1, 'Error: Could not open file %s\n', fn);
	error("Failed.");
end
nl = 8;
fprintf(fh, '# %s %s\n', comment, date());
if ~isempty(howto)
	fprintf(fh, '# %s\n', howto);
end
fprintf(fh, 'Object.Base.data.\"%s\" {\n', component);
fprintf(fh, '\tbytes \"\n');
for i = 1:nl:n_blob
	fprintf(fh, '\t\t');
	for j = 0:nl-1
		n = i + j;
		if n < n_blob
			fprintf(fh, '0x%02x,', blob8(n));
		end
		if n == n_blob
			fprintf(fh, '0x%02x"', blob8(n));
		end
	end
	fprintf(fh, '\n');
end
fprintf(fh, '}\n');
fclose(fh);
fprintf('Blob size %d was written to file %s\n', n_blob, fn);

end
