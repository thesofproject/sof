function eq = eq_blob_plot(blobfn, eqtype, fs, f, doplot)

% eq = eq_blob_plot(blobfn, eqtype, fs, f, doplot)
%
% Plot frequency response of IIR or FIR EQ coefficients blob
%
% Inputs
%
% blobfn - filename of the blob
% eqtype - 'iir' or 'fir', if omitted done via string search from blobfn
% fs     - sample rate, defaults to 48 kHz if omitted
% f      - frequency vector
% dpplot

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2016-2020, Intel Corporation. All rights reserved.
%
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

%% Handle input parameters
if nargin < 2
	if findstr(blobfn, '_fir_')
		eqtype = 'FIR';
	else
		eqtype = 'IIR';
	end
end

if nargin < 3
	fs = 48e3;
end

if nargin < 4 || isempty(f)
	eq.f = logspace(log10(10),log10(fs/2), 1000);
	f_single = [];
else
	% Freqz() needs two frequency points or more
	if length(f) < 2
		f_single = 1;
		eq.f = [f f];
	else
		f_single = 0;
		eq.f = f;
	end
end

if nargin < 5
	doplot = 1;
end

%% Read blob as 32 bit integers
blob = eq_blob_read(blobfn);

%% Group delay with grpdelay() is not working in Octave
do_group_delay = ~exist('OCTAVE_VERSION', 'builtin');

%% Decode and compute response
eq.m = [];
eq.gd = [];
switch lower(eqtype)
	case 'fir'
		hd = eq_fir_blob_decode(blob);
		eq.m = zeros(length(eq.f), hd.channels_in_config);
		for i = 1:hd.channels_in_config
			teq = eq_fir_blob_decode(blob, hd.assign_response(i));
			h = freqz(teq.b, 1, eq.f, fs);
			eq.m(:,i) = 20*log10(abs(h));
			if do_group_delay
				gd = grpdelay(teq.b, 1, eq.f, fs) / fs;
				eq.gd(:,i) = gd;
			end
		end

	case 'iir'
		hd = eq_iir_blob_decode(blob);
		eq.m = zeros(length(eq.f), hd.channels_in_config);
		for i = 1:hd.channels_in_config
			teq = eq_iir_blob_decode(blob, hd.assign_response(i));
			h = freqz(teq.b, teq.a, eq.f, fs);
			eq.m(:,i) = 20*log10(abs(h));
			if do_group_delay
				gd = grpdelay(teq.b, teq.a, eq.f, fs) / fs;
				eq.gd(:,i) = gd;
			end
		end
end

%% Optional plots
if doplot
	eq.fh(1) = figure;
	semilogx(eq.f, eq.m);
	ymin = max(min(min(eq.m)), -60);
	ymax = max(max(eq.m));
	axis([10 round(fs/2/10)*10 ymin-3 ymax+3 ]);
	grid on;
	channel_legends(hd.channels_in_config);
	xlabel('Frequency (Hz)');
	ylabel('Amplitude (dB)');
	title(blobfn, 'Interpreter', 'None');

	if do_group_delay
		eq.fh(2) = figure;
		semilogx(eq.f, eq.gd * 1e6);
		ax = axis();
		axis([10 round(fs/2/10)*10 ax(3:4) ]);
		grid on;
		channel_legends(hd.channels_in_config);
		xlabel('Frequency (Hz)');
		ylabel('Group delay (us)');
		title(blobfn, 'Interpreter', 'None');
	end
end

%% Trim avay the duplicated frequency point
if f_single
	eq.f = eq.f(1);
	eq.m = eq.m(1, :);
	if do_group_delay
		eq.gd = eq.gd(1, :);
	end
end

end

%% Helper functions
function channel_legends(n)

switch n
	case 2
		legend('ch1', 'ch2');
	case 4
		legend('ch1', 'ch2', 'ch3', 'ch4');
	case 6
		legend('ch1', 'ch2', 'ch3', 'ch4', 'ch5', 'ch6');
	case 8
		legend('ch1', 'ch2', 'ch3', 'ch4', 'ch5', 'ch6', 'ch7', 'ch8');
	otherwise
		error('Illegal number of channels found');
end

end
