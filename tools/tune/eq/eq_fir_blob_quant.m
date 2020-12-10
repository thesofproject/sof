function fbr = eq_fir_blob_quant(b, bits, strip_trailing_zeros)

%% Quantize FIR coefficients and return vector with length,
%  out shift, and coefficients to be used in the setup blob.
%
%  fbr = eq_fir_blob_resp(b, bits)
%  b - FIR coefficients
%  bits - optional number of bits, defaults to 16
%
%  fbr - vector with length, in shift, out shift, and quantized coefficients
%

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2016, Intel Corporation. All rights reserved.
%
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

b = (b(:))';

if nargin < 3
	strip_trailing_zeros = 1;
end

if nargin < 2
	bits = 16;
end

%% Quantize
[bq, shift] = eq_fir_quantize(b, bits);

%% Check trailing zeros
nf = length(bq);
nz = nf;
while bq(nz) == 0
	nz = nz - 1;
end
if nz < nf && strip_trailing_zeros
	nb = nz + 1;
	fprintf(1, 'Note: Filter length was reduced ');
	fprintf(1, 'to %d -> %d due to trailing zeros.\n', nf, nb);
	bq = bq(1:nb);
else
	nb = nf;
end

%% Make length multiple of four (optimized FIR core)
mod4 = mod(nb, 4);
if mod4 > 0
	pad = zeros(1,4-mod4);
	bqp = [bq pad];
	nnew = length(bqp);
	fprintf(1,'Note: Filter length was %d, padded length into %d.\n', ...
		nb, nnew);
else
	fprintf(1,'Note: Filter length is %d\n', nb);
	nnew = nb;
	bqp = bq;
end

%% Pack data into FIR coefficient format
%	int16_t length
%	int16_t out_shift
%	uint32_t reserved[4]
%	int16_t coef[]
fbr = [nnew shift 0 0 0 0 0 0 0 0 bqp];

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
