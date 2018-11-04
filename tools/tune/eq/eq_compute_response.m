function [m, ph, gd] = eq_compute_response(z, p, k, f, fs)

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

switch nargin
        case 3
                b = z;  % 1st arg
                f = p;  % 2nd arg
                fs = k; % 3rd arg
                h = freqz(b, 1, f, fs);
                m = 20*log10(abs(h));
                ph = 180/pi*angle(h);

                if length(b) == 1
                        gd = zeros(1, length(f));
                else
			if exist('OCTAVE_VERSION', 'builtin')
				% grpdelay() has some issue so better to not show a plot
				gd = NaN * zeros(1, length(f));
			else
				gd = 1/fs*grpdelay(b, 1, f, fs);
			end
                end
        case 5
                [b, a] = zp2tf(z, p, k);
                h = freqz(b, a, f, fs);
                m = 20*log10(abs(h));
                ph = 180/pi*angle(h);

                if length(z) == 0 && length(p) == 0
                        gd = zeros(1, length(f));
                else
			if exist('OCTAVE_VERSION', 'builtin')
				% grpdelay() has some issue so better to not show a plot
				gd = NaN * zeros(1, length(f));
			else
				gd = 1/fs*grpdelay(b, a, f, fs);
			end
                end
        otherwise
                error('Incorrect input parameters');
end

end
