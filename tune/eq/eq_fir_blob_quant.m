function fbr = eq_fir_blob_quant(b, bits)

%% Quantize FIR coefficients and return vector with length,
%  out shift, and coefficients to be used in the setup blob.
%
%  fbr = eq_fir_blob_resp(b, bits)
%  b - FIR coefficients
%  bits - optional number of bits, defaults to 16
%
%  fbr - vector with length, in shift, out shift, and quantized coefficients
%

%%
% Copyright (c) 2016, Intel Corporation
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

if nargin < 2
	bits = 16;
end

[bq, shift] = eq_fir_quantize(b, bits);

nb = length(bq);
mod4 = mod(nb, 4);
if mod4 > 0
	pad = zeros(1,4-mod4);
	bqp = [bq pad];
	nnew = length(bqp);
	fprintf(1,'Note: Filter length was %d, padded length into %d,\n', nb, nnew);
else
	nnew = nb;
	bqp = bq;
end

fbr = [nnew shift bqp];

end

function [bq, shift] = eq_fir_quantize(b, bits)

% [bq, shift] = eq_fir_quantize(b, bits)
%
% Inputs
% b - FIR coefficients
% bits - number bits for 2s complement coefficient
%
% Outputs
% bq - quantized coefficients
% shift - shift right parameter to apply after FIR computation to
% compensate for coefficients scaling
%

scale = 2^(bits-1);

%% Output shift for coefficients
m = max(abs(b));
shift = -ceil(log(m+1/scale)/log(2));
bsr = b*2^shift;

%% Quantize to Q1.bits-1 format, e.g. Q1.15 for 16 bits
bq = eq_coef_quant(bsr, bits, bits-1);

end
