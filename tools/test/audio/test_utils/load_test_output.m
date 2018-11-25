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

%%
% Copyright (c) 2017, Intel Corporation
% All rights reserved.
%
% Redistribution and use in source and binary forms, with or without
% modification, are permitted provided that the following conditions are met:
%   * Redistributions of source code must retain the above copyright
%     notice, this list of conditions and the following disclaimer.
%   * Redistributions in binary form must reproduce the above copyright
%     notice, this list of conditions and the following disclaimer in the
%     documentation and/or other materials provided with the distribution.
%   * Neither the name of the Intel Corporation nor the
%     names of its contributors may be used to endorse or promote products
%     derived from this software without specific prior written permission.
%
% THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
% AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
% IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
% ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
% LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
% CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
% SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
% INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
% CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
% ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
% POSSIBILITY OF SUCH DAMAGE.
%
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
%

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
if exist(test.fn_out)
        fprintf('Reading output data file %s...\n', test.fn_out);
	switch lower(test.fmt)
		case 'txt'
			out = load(test.fn_out);
		case 'raw'
			fh = fopen(test.fn_out, 'r');
			out = fread(fh, inf, bfmt);
			fclose(fh);
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
