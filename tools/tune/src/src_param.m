function cnv = src_param(fs1, fs2, coef_bits, q, gain)

% src_param - get converter parameters
%
% cnv = src_param(fs1, fs2, coef_bits, q)
%
% fs1       - input rate
% fs2       - output rate
% coef_bits - word length identifier
% q         - quality scale filter bandwidth and stopband attenuation,
%	      1 is default
% gain      - overall gain of SRC, defaults to -1 (dB) if omitted
%

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

% Note: firpm design with rs 93 dB, or kaiser with 69 dB rs is fair quality.
% Both give about -80 dB THD+N with 24 bit coefficients. With 16 bit
% coefficients THD+N is limited to about -76 dB.

if nargin < 5
	gain = -1;
end

if nargin < 4
	q = 1.0;
end

%% Copy input parameters
cnv.fs1 = fs1;
cnv.fs2 = fs2;
cnv.coef_bits = coef_bits;

%% FIR design
cnv.design = 'kaiser'; % Use firpm or kaiser

%% Default SRC quality
cnv.c_pb = q * 20/44.1; % Gives 20 kHz BW @ 44.1 kHz
cnv.c_sb = 0.5; % Start stopband at Fs/2
cnv.rs = 70; % Stopband attenuation in dB
cnv.rp = 0.1; % Passband ripple in dB
cnv.rp_tot = 0.1; % Max +/- passband ripple allowed, used in test script only
cnv.gain = gain; % Gain in decibels at 0 Hz


%% Constrain sub-filter lengths. Make subfilters lengths multiple of four
%  is a good assumption for processors.
cnv.filter_length_mult = 4;

%% Exceptions for quality
if min(fs1, fs2) > 80e3
	cnv.c_pb = 24e3/min(fs1, fs2); % 24 kHz BW for > 80 kHz
end

%% Sanity checks
if cnv.c_pb > 0.49
	error('Too wide passband');
end
if cnv.c_pb < 0.10
	error('Too narrow passband');
end
if cnv.rs > 160
	error('Too large stopband attenuation');
end
if cnv.rs < 40
	error('Too low stopband attenuation');
end
