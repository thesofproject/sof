function write_test_data(x, fn, bits, fmt, fs)

% SPDX-License-Identifier: BSD-3-Clause
% Copyright(c) 2019 Intel Corporation. All rights reserved.
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

switch lower(fmt)
	case 'wav'
		y = double(x) / 2^(bits-1);
		audiowrite(fn, y, fs, 'BitsPerSample', bits);
		return;
	case 'raw'
		fh = fopen(fn, 'wb');
	case 'txt'
		fh = fopen(fn, 'w');
	otherwise
		error('The file format must be raw or txt');
end
switch bits
	case 16
		bfmt = 'int16';
	case 24
		bfmt = 'int32';
	case 32
		bfmt = 'int32';
	otherwise
		error('The bits must be set to 16, 24, or 32');
end

%% Interleave the channels if two or more
sx = size(x);
xi = zeros(sx(1)*sx(2), 1, bfmt);
if sx(2) > 1
        for n=1:sx(2)
                xi(n:sx(2):end) = x(:,n);
        end
else
	xi(1:end) = x;
end

%% Write file and close
if strcmp(lower(fmt), 'raw')
	fwrite(fh, xi, bfmt);
else
	fprintf(fh,'%d\n', xi);
end
fclose(fh);

end
