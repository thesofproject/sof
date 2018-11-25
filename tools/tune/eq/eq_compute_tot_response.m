function eq = eq_compute_tot_response(eq)

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

% FIR response and combined IIR+FIR EQ
[eq.iir_eq_db, eq.iir_eq_ph, eq.iir_eq_gd] = eq_compute_response(eq.p_z, eq.p_p, eq.p_k, eq.f, eq.fs);
[eq.fir_eq_db, eq.fir_eq_ph, eq.fir_eq_gd] = eq_compute_response(eq.b_fir, eq.f, eq.fs);
eq.tot_eq_db = eq.iir_eq_db + eq.fir_eq_db;
eq.tot_eq_gd = eq.iir_eq_gd + eq.fir_eq_gd;

% Simulated equalized result
eq.m_eqd = eq.m_db + eq.tot_eq_db; % Simulated response
eq.m_eqd_s = eq.m_db_s + eq.tot_eq_db; % Smoothed simulated response
eq.gd_eqd = eq.gd_s' + eq.tot_eq_gd;

% Align
[eq.m_eqd_s, adjust, p] = eq_align(eq.f, eq.m_eqd_s, eq.f_align, eq.db_align);
eq.m_eqd = eq.m_eqd + adjust;
eq.m_eq_loss = eq.tot_eq_db(p);
eq.m_eqd_abs = eq.m_db_abs+eq.m_eq_loss;

end
