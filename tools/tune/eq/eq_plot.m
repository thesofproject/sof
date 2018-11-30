function eq_plot(eq, fn)

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

if nargin < 2
        fn = 1;
end

%% Raw measured response
if length(eq.raw_m_db) > 2
        % Raw without EQ
        fh=figure(fn); fn = fn+1;
	f1 = min(eq.raw_f);
	f2 = max(eq.raw_f);
	i1 = find(eq.f > f1, 1, 'first') - 1;
	i2 = find(eq.f > f2, 1, 'first') - 1;
	idx = i1:i2;
        semilogx(eq.f(idx), eq.m_db(idx), eq.f(idx), eq.m_db_s(idx));
        grid on;
        ax=axis; axis([eq.p_fmin eq.p_fmax min(max(ax(3:4), -40), 20)]);
        legend('Raw','Smoothed');
        xlabel('Frequency (Hz)');
        ylabel('Magnitude (dB)');
        tstr = sprintf('Imported frequency response: %s', eq.name);
        title(tstr);

        % Simulated with EQ
        fh=figure(fn); fn = fn+1;
        semilogx(eq.f(idx), eq.m_eqd(idx), eq.f(idx), eq.m_eqd_s(idx));
        grid on;
        ax=axis; axis([eq.p_fmin eq.p_fmax min(max(ax(3:4), -40), 20)]);
        legend('Raw','Smoothed');
        xlabel('Frequency (Hz)');
        ylabel('Magnitude (dB)');
        tstr = sprintf('Simulated frequency response: %s', eq.name);
        title(tstr);
end

%% Filter responses
fh=figure(fn); fn = fn+1;
i1k = find(eq.f > 1e3, 1, 'first') - 1;
offs_tot = -eq.tot_eq_db(i1k);
offs_fir = -eq.fir_eq_db(i1k);
offs_iir = -eq.iir_eq_db(i1k);
if eq.enable_fir && eq.enable_iir
        semilogx(eq.f, eq.err_db_s, eq.f, eq.tot_eq_db + offs_tot, ...
                eq.f, eq.iir_eq_db + offs_iir, '--',...
                eq.f, eq.fir_eq_db + offs_fir, '--');
        legend('Target', 'Combined', 'IIR', 'FIR', 'Location', 'NorthWest');
end
if eq.enable_fir && eq.enable_iir == 0
        semilogx(eq.f, eq.err_db_s, eq.f, eq.fir_eq_db + offs_fir);
        legend('Target', 'FIR', 'Location', 'NorthWest');
end
if eq.enable_fir == 0 && eq.enable_iir
        semilogx(eq.f, eq.err_db_s, eq.f, eq.iir_eq_db + offs_iir);
        legend('Target', 'IIR', 'Location', 'NorthWest');
end
grid on;
ax=axis; axis([eq.p_fmin eq.p_fmax min(max(ax(3:4), -40), 40)]);
xlabel('Frequency (Hz)');
ylabel('Magnitude (dB)');
tstr = sprintf('Filter target vs. achieved response: %s', eq.name);
title(tstr);

%% FIR filter
if length(eq.b_fir) > 1
        % Response
	fh=figure(fn); fn = fn+1;
	semilogx(eq.f, eq.fir_eq_db);
	grid on;
	xlabel('Frequency (Hz)');
	ylabel('Magnitude (dB)');
        ax = axis; axis([eq.p_fmin eq.p_fmax max(ax(3:4), -40)]);
        tstr = sprintf('FIR filter absolute response: %s', eq.name);
        title(tstr);

        % Impulse response / coefficients
        fh=figure(fn); fn = fn+1;
	stem(eq.b_fir);
	grid on;
	xlabel('Coefficient #');
	ylabel('Coefficient value');
        tstr = sprintf('FIR filter impulse response: %s', eq.name);
        title(tstr);

end

%% IIR filter
if length(eq.p_z) > 1 || length(eq.p_p) > 1
        % Response
	fh=figure(fn); fn = fn+1;
	semilogx(eq.f, eq.iir_eq_db);
	grid on;
	xlabel('Frequency (Hz)');
	ylabel('Magnitude (dB)');
        ax = axis; axis([eq.p_fmin eq.p_fmax max(ax(3:4), -40)]);
        tstr = sprintf('IIR filter absolute response: %s', eq.name);
        title(tstr);

        % Polar
        fh=figure(fn); fn = fn+1;
        zplane(eq.p_z, eq.p_p);
        grid on;
        tstr = sprintf('IIR zeros and poles: %s', eq.name);
        title(tstr);

        % Impulse
        ti = 50e-3;
        x = zeros(1, ti * round(eq.fs));
        x(1) = 1;
        sos = zp2sos(eq.p_z, eq.p_p, eq.p_k);
        y = sosfilt(sos, x);
	fh=figure(fn); fn = fn+1;
        t = (0:(length(x)-1)) / eq.fs;
        plot(t, y);
        grid on;
        xlabel('Time (s)');
        ylabel('Sample value');
        tstr = sprintf('IIR filter impulse response: %s', eq.name);
        title(tstr);
end

%% Group delay
if ~exist('OCTAVE_VERSION', 'builtin')
	% Skip plot if running in Octave due to incorrect result
	fh=figure(fn); fn = fn+1;
	if eq.enable_fir && eq.enable_iir
		semilogx(eq.f, eq.tot_eq_gd * 1e3, ...
			 eq.f, eq.fir_eq_gd * 1e3, '--', ...
			 eq.f, eq.iir_eq_gd * 1e3, '--');
		legend('Combined','FIR','IIR');
	end
	if eq.enable_fir && eq.enable_iir == 0
		semilogx(eq.f, eq.fir_eq_gd * 1e3);
		legend('FIR');
	end
	if eq.enable_fir == 0 && eq.enable_iir
		semilogx(eq.f, eq.iir_eq_gd * 1e3);
		legend('IIR');
	end
	grid on;
	xlabel('Frequency (Hz)');
	ylabel('Group delay (ms)');
	ax = axis; axis([eq.p_fmin eq.p_fmax ax(3:4)]);
	tstr = sprintf('Filter group delay: %s', eq.name);
	title(tstr);
end

end
