function [x, nx] = load_test_output(test)

%% [x, n] = load_test_output(t)
%
% Input
% t.fn_out     - file name to load
% t.bits_out   - word length of data
% t.fmt        - file format 'raw' or 'txt
% t.ch         - channel to extract
% t.nch        - number of channels in (interleaved) data
%
% Output
% x - samples
% n - number of samples
%

% SPDX-License-Identifier: BSD-3-Clause
% Copyright(c) 2017 Intel Corporation. All rights reserved.
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

%% Integer type for binary files
switch test.bits_out
	case 16
		bfmt = 'int16';
	case 24
		bfmt = 'int32';
	case 32
		bfmt = 'int32';
	otherwise
		error('Bits_out must be 16, 24, or 32.');
end

%% Check that output file exists
if exist(fullfile('.', test.fn_out),'file') == 2
	fprintf('Reading output data file %s...\n', test.fn_out);
	switch lower(test.fmt)
		case 'txt'
			out = load(test.fn_out);
		case 'raw'
			fh = fopen(test.fn_out, 'r');
			out = fread(fh, inf, bfmt);
			fclose(fh);
		case 'wav'
			[x0, fs] = audioread(test.fn_out);
			x = x0(:,test.ch);
			sx = size(x);
			nx = sx(1);
			return
		otherwise
			error('Fmt must be raw or txt.');
	end
else
	out = [];
end

%% Exctract channels to measure
scale = 1/2^(test.bits_out-1);
lout = length(out);
nx = floor(lout/test.nch);
x = zeros(nx,length(test.ch));
j = 1;
for ch = test.ch
        x(:,j) = out(ch:test.nch:end)*scale;
        j = j + 1;
end

end
