function sof_crossover_plot_freq(lp, hp, fs, num_sinks);
% Plot the transfer function of each sink. We need to reconstruct a filter
% that represents the path the samples go through for each sinks.
% Example 4-way crossover:
%
%                              o---- LR4 LO-PASS --> y1(n)
%                              |
%           o--- LR4 LO-PASS --o
%           |                  |
%           |                  o--- LR4 HI-PASS --> y2(n)
%  x(n) --- o
%           |                  o--- LR4 LO-PASS --> y3(n)
%           |                  |
%           o--- LR4 HI-PASS --o
%                              |
%                              o--- LR4 HI-PASS --> y4(n)
%
% Then to plot the transferfn for y1 we would create a filter such as:
%  x(n) ---> LR4 LO-PASS --> LR4 LO-PASS --> y1(n)

figure;

f = linspace(1, fs/2, 500);

if num_sinks == 2
	h1 = cascade_bqs_fr(f, fs, lp(1), lp(1));
	h2 = cascade_bqs_fr(f, fs, hp(1), hp(1));

	subplot(2 ,1, 2)
	freqz_plot(f, h1)
	subplot(2, 1, 1)
	plot_mag_resp(f, h1, h2)
end

if num_sinks == 3
	% First LR4 Low Pass Filters
	h1 = cascade_bqs_fr(f, fs, lp(1), lp(1));
	% Second LR4 Filters
	tmp = cascade_bqs_fr(f, fs, lp(2), lp(2));
	tmp2 = cascade_bqs_fr(f, fs, hp(2), hp(2));
	% Merge the second LR4 Filters
	tmp = tmp + tmp2;
	% Cascade the First LR4 and the result of the previous merge
	h1 = h1.*tmp;

	h2 = cascade_bqs_fr(f, fs, hp(1), hp(1), lp(3), lp(3));
	h3 = cascade_bqs_fr(f, fs, hp(1), hp(1), hp(3), hp(3));

	subplot(2, 1, 2)
	plot_phase_resp(f, h1, h2, h3)
	subplot(2, 1, 1)
	plot_mag_resp(f, h1, h2, h3)
end

if num_sinks == 4
	h1 = cascade_bqs_fr(f, fs, lp(2), lp(2), lp(1), lp(1));
	h2 = cascade_bqs_fr(f, fs, lp(2), lp(2), hp(1), hp(1));
	h3 = cascade_bqs_fr(f, fs, hp(2), hp(2), lp(3), lp(3));
	h4 = cascade_bqs_fr(f, fs, hp(2), hp(2), hp(3), hp(3));

	subplot(2, 1, 2)
	plot_phase_resp(f, h1, h2, h3, h4)
	subplot(2, 1, 1)
	plot_mag_resp(f, h1, h2, h3, h4)
end
end

function [h12, w] = cascade_bqs_fr(f, fs, varargin)
bq1 = varargin{1};
bq2 = varargin{2};
[h1, w1] = freqz(bq1.b, bq1.a, f, fs);
[h2, w2] = freqz(bq2.b, bq2.a, f, fs);
h12 = h1.*h2;
for i=3:length(varargin)
	bq = varargin{i};
	[h1, w] = freqz(bq.b, bq.a, f, fs);
	h12 = h12.*h1;
end
end

function plot_phase_resp(f,varargin)
n = length(varargin);
labels = cellstr(n);
hold on
grid on
for i=1:n
	h = varargin{i};
	semilogx(f, unwrap(arg(h)) * 180 / pi)
	labels(i) = sprintf("out%d", i);
end
legend(labels, 'Location', 'NorthEast')
xlabel('Frequency (Hz)');
ylabel('Phase Shift (degrees)');
tstr = "Crossover Filter Phase Response";
title(tstr);
end

function plot_mag_resp(f,varargin)
n = length(varargin);
labels = cellstr(n);
hold on
grid on
for i=1:n
	h = varargin{i};
	semilogx(f,20*log10(h))
	labels(i) = sprintf("out%d", i);
end
legend(labels, 'Location', 'NorthEast')
xlabel('Frequency (Hz)');
ylabel('Magnitude (dB)');
tstr = "Crossover Filter Magnitude Response";
title(tstr);
end
