function iir_resp = eq_iir_blob_quant(eq_z, eq_p, eq_k)

%% Convert IIR coefficients to 2nd order sections and quantize
%
%  iir_resp = eq_iir_blob_quant(z, p, k)
%
%  z - zeros
%  p - poles
%  k - gain
%
%  iir_resp - vector to setup an IIR equalizer with number of sections, shifts,
%  and quantized coefficients
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

%% Settings
bits_iir = 32; % Q2.30
qf_iir = 30;
bits_gain = 16; % Q2.14
qf_gain = 14;
scale_max = -6; % dB, scale biquads peak gain to this
plot_pz = 0;
plot_fr = 0;

%% Convert IIR to 2nd order sections
%  This a simple implementation of zp2sos() function. It is not used here due
%  to utilization of rather strong scaling and resulting low SNR with the
%  available word length in EQ in SOF. This poles and zeros allocation to
%  biquads is base only in ascending sort of angular frequency.
sz = length(eq_z);
sp = length(eq_p);
sk = length(eq_k);
nb = max(sz, sp)/2;
az = angle(eq_z);
ap = angle(eq_p);
[~, iz] = sort(abs(az));
[~, ip] = sort(abs(ap));
eq_z = eq_z(iz);
eq_p = eq_p(ip);
sos = zeros(nb, 6);
for i = 1:nb
        j = 2*(i-1)+1;
        if i == 1
                [b, a] = zp2tf(eq_z(j:j+1), eq_p(j:j+1), eq_k);
        else
                [b, a] = zp2tf(eq_z(j:j+1), eq_p(j:j+1), 1);
        end
        sos(i,1:3) = b;
        sos(i,4:6) = a;
end
gain = 1;

%% Convert 2nd order sections to SOF parameters format and scale the biquads
%  with criteria below (Gain max -6 dB at any frequency). Then calculate
%  scaling shifts and finally gain multiplier for output.
sz = size(sos);
nbr_sections = sz(1);
n_section_header = 6; % Two plus reserved[4] in ABI
n_section = 7;
iir_resp = int32(zeros(1,n_section_header+nbr_sections*n_section));
iir_resp(1) = nbr_sections;
iir_resp(2) = nbr_sections; % Note: All sections in series

scale_max_lin = 10^(scale_max/20);
for n=1:nbr_sections
        b = sos(n,1:3);
        a = sos(n,4:6);
        if plot_pz
                figure
                zplane(b,a);
                tstr = sprintf('SOS %d poles and zeros', n);
                title(tstr);
        end

        np = 1024;
        [h, w] = freqz(b, a, np);
        hm = max(abs(h));
        scale = scale_max_lin/hm;
        gain_remain = 1/scale;
        gain = gain*gain_remain;
        b = b * scale;
        ma = max(abs(a));
        mb = max(abs(b));
        if plot_fr
                figure
                [h, w] = freqz(b, a, np);
                plot(w, 20*log10(abs(h))); grid on;
                xlabel('Frequency (w)');
                ylabel('Magnitude (dB)');
                tstr = sprintf('SOS %d frequency response', n);
                title(tstr);
        end

        %% Apply remaining gain at last section output
        if n == nbr_sections
                section_shift = -fix(log(gain)/log(2));
                section_gain= gain/2^(-section_shift);
        else
                section_shift = 0;
                section_gain = 1;
        end

        %% Note: Invert sign of a!
        %% Note:  a(1) is omitted, it's always 1
        m = n_section_header+(n-1)*n_section+1;
        iir_resp(m:m+1) = eq_coef_quant(-a(3:-1:2), bits_iir, qf_iir);
        iir_resp(m+2:m+4) =  eq_coef_quant( b(3:-1:1), bits_iir, qf_iir);
        iir_resp(m+5) = section_shift;
        iir_resp(m+6) = eq_coef_quant( section_gain, bits_gain, qf_gain);

	%fprintf('sec=%d, shift=%d, gain=%f\n', n, section_shift, section_gain);

end

end
