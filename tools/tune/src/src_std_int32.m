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

fs1 = [8 11.025 12 16 22.05 24 32 44.1 48 50 64 88.2 96 176.4 192] * 1e3;

fs2 = [8 11.025 12 16 22.05 24 32 44.1 48 50] * 1e3;

fs_matrix = [ 0 0 0 1 0 1 1 0 1 0 ; ...
	      0 0 0 0 0 0 0 0 1 0 ; ...
	      0 0 0 0 0 0 0 0 1 0 ; ...
	      1 0 0 0 0 1 1 0 1 0 ; ...
	      0 0 0 0 0 0 0 0 1 0 ; ...
	      1 0 0 1 0 0 1 0 1 0 ; ...
	      1 0 0 1 0 1 0 0 1 0 ; ...
	      0 0 0 0 0 0 0 0 1 0 ; ...
	      1 1 1 1 1 1 1 1 0 1 ; ...
	      0 0 0 0 0 0 0 0 1 0 ; ...
	      0 0 0 0 0 0 0 0 1 0 ; ...
	      0 0 0 0 0 0 0 0 1 0 ; ...
	      0 0 0 0 0 0 0 0 1 0 ; ...
	      0 0 0 0 0 0 0 0 1 0 ; ...
	      0 0 0 0 0 0 0 0 1 0 ];

cfg.ctype = 'int32';
cfg.profile = 'std';
cfg.quality = 1.0;
cfg.speed = 0;
cfg.gain = -1; % Make gain -1 dB

src_generate(fs1, fs2, fs_matrix, cfg);
