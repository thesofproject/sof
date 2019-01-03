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

[n_fail, n_pass] = src_test(16, 16);
if n_fail > 0 || n_pass < 1
	fprintf('Error: SRC test with 16 bits data failed.\n');
        quit(1)
end
[n_fail, n_pass] = src_test(24, 24);
if n_fail > 0 || n_pass < 1
	fprintf('Error: SRC test with 24 bits data failed.\n');
        quit(1)
end
[n_fail, n_pass] = src_test(32, 32);
if n_fail > 0 || n_pass < 1
	fprintf('Error: SRC test with 32 bits data failed.\n');
        quit(1)
end

fprintf('Success: SRC test with 16, 24 and 32 bits data passed.\n');
