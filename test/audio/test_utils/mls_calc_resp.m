function [f, m_db, b] = mls_calc_resp(csv_fn, ref_wfn, meas_wfn, t_tot, np, f_lo, f_hi)

%% Calculate frequency response from MLS recordings
%
%  [f, m, b] = calc_freq_resp(csv_fn, ref_fn, meas_fn, t_tot, np, f_lo, f_hi)
%
%  Inputs parameters:
%  csv_fn  - File name for CSV format response output
%  ref_fn  - Reference MLS wave file
%  meas_fn - Captured MSL wave file
%  t_tot   - Time window of MLS analysis in s, e.g. 5 - 10 ms
%  np      - Number of frequency points
%  f_lo    - Lower frequency limit for analysis
%  f_hi    - Upper frequency limit for analysis

%%
% Copyright (c) 2018, Intel Corporation
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

	%% Read audio files
	[xr, fs] = audioread(ref_wfn);
	[xm, fsm]  = audioread(meas_wfn);
	sm = size(xm);
	nch = sm(2);

	if (fs ~= fsm)
		error('Reference and measured sample rates do not match.');
	end

	ind_imp = zeros(1, nch);
	for ch=1:nch
		[acor(:,ch),lag] = xcorr(xm(:,ch), xr, 'coeff');
		[~,I] = max(abs(acor(:,ch)));
		timeDiff = lag(I);
	        % Crosscorrelation maximum points to max of impulse response.
	        % Use it beginning of impulse response
		ind_imp(ch) = find(lag == timeDiff);
	end
	% Keep the channels time aligned by using average of max as reference
	ind_imp_avg = round(sum(ind_imp)/nch);

	b = [];
	nwin_half = round(t_tot*fs);
	nwin_tot = 2*nwin_half;
	win = hanning(nwin_tot);
	for ch = 1:nch
		% All channels are windowed from the same location
		p1 = ind_imp_avg-nwin_half;
		p2 = p1 + nwin_tot-1;
		acor_windowed = acor(p1:p2,ch) .* win;
		b(:,ch) = acor_windowed;
	end


	%% Compute frequency response
	f = logspace(log10(f_lo),log10(f_hi),np);
	for ch=1:nch
		h(:,ch) = freqz(b(:,ch), 1, f, fs);
	end
	m_db = 20*log10(abs(h));

	fh = fopen(csv_fn, 'w');
	for i = 1:np
		fprintf(fh, '%10.3f', f(i));
		for j = 1:nch
			fprintf(fh, ', %8.3f', m_db(i, j));
		end
		fprintf(fh, '\n');
	end
	fclose(fh);

end
