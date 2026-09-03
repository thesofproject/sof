function x = multitone( fs, f, amp, tlength )

%%
% Create single or multiple sine wave(s)
%
% x = multitone(fs, f, a, t);
%
% fs     - Sample rate
% f      - Frequency (Hz)
% a      - Amplitude (lin)
% t      - Length in seconds
%
% Example:
% x = multitone(48000, [997 1997], 10.^([-26 -46]/20), 1.0);
%

% SPDX-License-Identifier: BSD-3-Clause
% Copyright(c) 2017 Intel Corporation. All rights reserved.
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

n = round(fs*tlength);
t = (0:n-1)/fs;
nf = length(f);
if nf > 1
	ph = rand(nf, 1)*2*pi;
else
	ph = 0;
end

x = zeros(n, 1);
x(:,1) = amp(1)*sin(2*pi*f(1)*t);
for i=2:length(f)
	x(:,1) = x(:,1) + amp(i)*sin(2*pi*f(i)*t+ph(i))';
end

end
