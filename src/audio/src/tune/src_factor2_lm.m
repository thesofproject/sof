function [l1, m1, l2, m2] = src_factor2_lm(fs1, fs2)

% factor2_lm - factorize input and output sample rates ratio to fraction l1/m2*l2/m2
%
% [l1, m1, l2, m2] = factor2_lm(fs1, fs2)
%
% fs1 - input sample rate
% fs2 - output sample rate
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


%% The same sample rate?
if abs(fs1-fs2) < eps
        l1 = 1; m1 = 1; l2 = 1; m2 = 1;
        return
end

%% Find fraction, and factorize
k = gcd(fs1, fs2);
l = fs2/k;
m = fs1/k;
[l01, l02] = factor2(l);
[m01, m02] = factor2(m);

%% Hand fixing for some ratios, guide to reuse some common filters
if (l==147) && (m==640)
        l01 = 7; m01 = 20; l02 = l/l01; m02 = m/m01; % 192 to 44.1, 48 to 11.025
end
if (l==147) && (m==320)
        l01 = 7; m01 = 8; l02 = l/l01; m02 = m/m01; % 96 to 44.1
end
if (l==147) && (m==160)
        l01 = 7; m01 = 8; l02 = l/l01; m02 = m/m01; % 48 to 44.1
end
if (l==160) && (m==147)
        l01 = 8; m01 = 7; l02 = l/l01; m02 = m/m01; % 44.1 to 48
end
if (l==320) && (m==147)
        l01 = 8; m01 = 7; l02 = l/l01; m02 = m/m01; % 44.1 to 96
end
if (l==4) && (m==3)
        l01 = 4; m01 = 3; l02 = l/l01; m02 = m/m01; % 24 to 32, no 2 stage
end
if (l==3) && (m==4)
        l01 = 3; m01 = 4; l02 = l/l01; m02 = m/m01; % 24 to 32, no 2 stage
end


r11 = l01/m01;
r22 = l02/m02;
r12 = l01/m02;
r21 = l02/m01;
fs3 = [r11 r12 r21 r22].*fs1;


if fs1 > fs2 % Decrease sample rate, don't go below output rate
        idx = find(fs3 >= fs2);
        if length(idx) < 1
                error('Cant factorise interpolations');
        end
        fs3_possible = fs3(idx);
        delta = fs3_possible-fs2; % Fs3 that is nearest to fs2
        idx = find(delta == min(delta), 1, 'first');
        fs3_min = fs3_possible(idx);
        idx = find(fs3 == fs3_min, 1, 'first');
        switch idx
                case 1, l1 = l01; m1 = m01; l2 = l02; m2 = m02;
                case 2, l1 = l01; m1 = m02; l2 = l02; m2 = m01;
                case 3, l1 = l02; m1 = m01; l2 = l01; m2 = m02;
                case 4, l1 = l02; m1 = m02; l2 = l01; m2 = m01;
        end
else % Increase sample rate, don't go below input rate
        idx = find(fs3 >= fs1);
        if length(idx) < 1
                error('Cant factorise interpolations');
        end
        fs3_possible = fs3(idx);
        delta = fs3_possible-fs1; % Fs2 that is nearest to fs1
        idx = find(delta == min(delta), 1, 'first');
        fs3_min = fs3_possible(idx);
        idx = find(fs3 == fs3_min, 1, 'first');
        switch idx
                case 1, l1 = l01; m1 = m01; l2 = l02; m2 = m02;
                case 2, l1 = l01; m1 = m02; l2 = l02; m2 = m01;
                case 3, l1 = l02; m1 = m01; l2 = l01; m2 = m02;
                case 4, l1 = l02; m1 = m02; l2 = l01; m2 = m01;
        end
end

%% If 1st stage is 1:1
if (l1 == 1) && (m1 == 1)
        l1 = l2;
        m1 = m2;
        l2 = 1;
        m2 = 1;
end

f1=l/m;
f2=l1/m1*l2/m2;
if abs(f1 - f2) > 1e-6
        error('Bug in factorization code!');
end

end


function [a, b]=factor2(c)
x = round(sqrt(c));
t1 = x:2*x;
d1 = c./t1;
idx1 = find(d1 == floor(d1), 1, 'first');
a1 = t1(idx1);
t2 = x:-1:floor(x/2);
d2 = c./t2;
idx2 = find(d2 == floor(d2), 1, 'first');
a2 = t2(idx2);
if (a1-x) < (x-a2)
        a = a1;
else
        a = a2;
end
b = c/a;
end
