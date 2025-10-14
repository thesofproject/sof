% [ceps, t, n] = decode_ceps(fn, num_ceps, num_channels)
%
% Input
%   fn - File with MFCC data in .raw or .wav format
%   num_ceps - number of cepstral coefficients per frame
%   num_channels - needed for .raw format, omit for .wav
%
% Outputs
%   ceps - cepstral coefficients
%   t - time vector for plotting
%   n - ceps 1..num_ceps vector for plotting

% SPDX-License-Identifier: BSD-3-Clause
% Copyright(c) 2022 Intel Corporation. All rights reserved.

function [ceps, t, n] = decode_ceps(fn, num_ceps, num_channels)

if nargin < 3
	num_channels = 1;
end

% MFCC stream
fs = 16e3;
qformat = 7;
magic = [25443 28006]; % ASCII 'mfcc' as int16

% Load output data
[data, num_channels] = get_file(fn, num_channels);

idx1 = find(data == magic(1));
idx = [];
for i = 1:length(idx1)
	if data(idx1(i) + 1) == magic(2)
		idx = [idx idx1(i)];
	end
end

if isempty(idx)
	error('No magic value markers found from stream');
end

period_ceps = idx(2)-idx(1);
num_frames = length(idx);
t_ceps = period_ceps / num_channels / fs;
t = (0:num_frames -1) * t_ceps;
n = 1:num_ceps;

ceps = zeros(num_ceps, num_frames);
for i = 1:num_frames
	i1 = idx(i) + 2;
	i2 = i1 + num_ceps - 1;
	ceps(:,i) = data(i1:i2) / 2^qformat;
end

figure;
surf(t, n, ceps, 'EdgeColor', 'none');
colormap(jet);
view(45, 60)
tstr = sprintf('SOF MFCC cepstral coefficients (%s)', fn);
title(tstr, 'Interpreter', 'None');
xlabel('Time (s)');
ylabel('Cepstral coef #');

end

function [data, num_channels] = get_file(fn, num_channels)

[~, ~, ext] = fileparts(fn);

switch lower(ext)
	case '.raw'
		fh = fopen(fn, 'r');
		data = fread(fh, 'int16');
		fclose(fh);
	case '.wav'
		tmp = audioread(fn, 'native');
		t = whos('tmp');
		if ~strcmp(t.class, 'int16');
			error('Only 16-bit wav file format is supported');
		end
		s = size(tmp);
		num_channels = s(2);
		if num_channels > 1
			data = int16(zeros(prod(s), 1));
			for i = 1:num_channels
				data(i:num_channels:end) = tmp(:, i);
			end
		end
	otherwise
		error('Unknown audio format');
end

end
