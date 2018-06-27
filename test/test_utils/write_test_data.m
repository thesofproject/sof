function write_test_data(x, fn, bits, fmt)

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

switch lower(fmt)
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
