function eq = sof_eq_norm(eq)

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

%% Normalize loudness of EQ by computing weighted average of linear
%  response. Find scale need to make the average one.
w_lin = level_norm_fweight(eq.f);
m_lin_fir = 10.^(eq.fir_eq_db/20);
m_lin_iir = 10.^(eq.iir_eq_db/20);
i1k = find(eq.f > 1e3, 1, 'first') - 1;
m_max_fir = max(eq.fir_eq_db);
m_max_iir = max(eq.iir_eq_db);
sens_fir = sum(m_lin_fir.*w_lin)/sum(w_lin);
sens_iir = sum(m_lin_iir.*w_lin)/sum(w_lin);
g_offs_iir = 10^(eq.iir_norm_offs_db/20);
g_offs_fir = 10^(eq.fir_norm_offs_db/20);

%% Determine scaling gain
switch lower(eq.iir_norm_type)
        case 'loudness'
                g_iir = 1/sens_iir;
        case '1k'
                g_iir = 1/m_lin_iir(i1k);
        case 'peak'
                g_iir = 10^(-m_max_iir/20);
        otherwise
                error('Requested IIR normalization is not supported');
end
switch lower(eq.fir_norm_type)
        case 'loudness'
                g_fir = 1/sens_fir;
        case '1k'
                g_fir = 1/m_lin_fir(i1k);
        case 'peak'
                g_fir = 10^(-m_max_fir/20);
        otherwise
                error('Requested FIR normalization is not supported');
end

%% Adjust FIR and IIR gains if enabled
if eq.enable_fir
        eq.b_fir = eq.b_fir * g_fir * g_offs_fir;
end
if eq.enable_iir
        eq.p_k = eq.p_k * g_iir * g_offs_iir;
end

%% Re-compute response after adjusting gain
eq = sof_eq_compute_tot_response(eq);

end

function m_lin = level_norm_fweight(f)

%% w_lin = level_norm_fweight(f)
%
% Provides frequency weight that is useful in normalization of
% loudness of effect like equalizers. The weight consists of pink noise
% like spectral shape that attenuates higher frequencies 3 dB per
% octave. The low frequencies are shaped by 2nd order high-pass
% response at 20 Hz and higher frequencies by 3rd order low-pass at 20
% kHz.
%
% Note: This weight may have similarity with a standard defined test signal
% characteristic for evaluating music player output levels but this is not
% an implementation of it. This weight may help in avoiding a designed EQ to
% exceed safe playback levels in othervise compliant device.
%
% Input f - frequencies vector in Hz
%
% Output w_lin  - weight curve, aligned to peak of 1.0 in the requested f
%

[z_hp, p_hp] = butter(2, 22.4,'high','s');
[z_lp, p_lp] = butter(3, 22.4e3,'s');
z_bp = conv(z_hp, z_lp);
p_bp = conv(p_hp, p_lp);
h = freqs(z_bp, p_bp, f);
m0 = 20*log10(abs(h));
noct = log(max(f)/min(f))/log(2);
att = 3*noct;
nf = length(f);
w = zeros(1,nf);
w = w - linspace(0,att,nf);
m = m0 + w;
m = m-max(m);
m_lin = 10.^(m/20);

end
