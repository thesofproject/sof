function p = sof_eq_defaults

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

% Misc
p.plot_figs = 0;
p.fn = 'configuration.txt';
p.fs = 48e3;
p.f_align = 997;
p.db_align = 0;
p.logsmooth_plot = 1/3; % Smooth over 1/3 octaves
p.logsmooth_eq = 1/3; % Smooth over 1/3 octaves
p.np_fine = 5000; % 5000 point design grid

% Parametric EQ IDs
p.PEQ_HP1 = 1; p.PEQ_HP2 = 2; p.PEQ_LP1 = 3; p.PEQ_LP2 = 4;
p.PEQ_LS1 = 5; p.PEQ_LS2 = 6; p.PEQ_HS1 = 7; p.PEQ_HS2 = 8;
p.PEQ_PN2 = 9; p.PEQ_LP4 = 10; p.PEQ_HP4 = 11; p.PEQ_LP2G = 12;
p.PEQ_HP2G = 13; p.PEQ_BP2 = 14; p.PEQ_NC2 = 15; p.PEQ_LS2G = 16;
p.PEQ_HS2G = 17;

% FIR constraints
p.fmin_fir = 200; %
p.fmax_fir = 15e3; %
p.amax_fir =  20;
p.amin_fir = -20;
p.fir_beta = 8;
p.fir_length = 63;
p.fir_minph = 0; % 0 = linear phase, 1 = minimum phase
% Adjust fmin_fir, fmax_fir automatically for least gain loss in FIR
p.fir_autoband = 1;
p.enable_fir = 0;

% IIR conf
p.iir_biquads_max = 8;
p.enable_iir = 0;

% Initialize other fields those are computed later to allow use of struct
% arrays
p.raw_f = [10 100e3];
p.raw_m_db = [0 0];
p.raw_gd_s = [];
p.f = [];
p.m_db = [];
p.gd_s = [];
p.num_responses = 0;
p.m_db_s = [];
p.gd_s_s = [];
p.logsmooth_noct = 0;
p.t_z = [];
p.t_p = [];
p.t_k = [];
p.t_db = [];
p.m_db_abs = 0;
p.m_db_offs = 0;
p.raw_m_noalign_db = [];
p.err_db = [];
p.p_z = [];
p.p_p = [];
p.p_k = [];
p.iir_eq_db = [];
p.iir_eq_ph = [];
p.iir_eq_gd = [];
p.err2_db = [];
p.t_fir_db = [];
p.b_fir = [];
p.fir_eq_db = [];
p.fir_eq_ph = [];
p.fir_eq_gd = [];
p.tot_eq_db = [];
p.tot_eq_gd = [];
p.m_eqd = [];
p.gd_eqd = [];
p.m_eq_loss = 0;
p.m_eqd_abs = 0;
p.sim_m_db = [];
p.parametric_target_response = [];
p.target_f = [10 100e3];
p.target_m_db = [0 0];
p.fir_compensate_iir = 1;
p.p_fmin = 10;
p.p_fmax = 30e3;
p.name = '';
p.iir_norm_type = 'loudness'; % loudness/peak/1k
p.iir_norm_offs_db = 0;
p.fir_norm_type = 'loudness'; % loudness/peak/1k
p.fir_norm_offs_db = 0;

end
