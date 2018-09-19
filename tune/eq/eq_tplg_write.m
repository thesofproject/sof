function eq_tplg_write(fn, blob8, eqtype)

%%
% Copyright (c) 2018, Intel Corporation
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

%% Pad blob length to multiple of four bytes
n_orig = length(blob8);
n_new = ceil(n_orig/4)*4;
blob8_new = zeros(1, n_new);
blob8_new(1:n_orig) = blob8;

%% Write blob
fh = fopen(fn, 'w');
nl = 8;
fprintf(fh, '`SectionData."''N_EQ_%s($1)`_data_bytes" {''\n', ...
       upper(eqtype));
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
fprintf(fh, '`}''\n');
fclose(fh);

end
