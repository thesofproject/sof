% [ceps, t, n, vad, energy, noise_energy, frame_number] = decode_ceps(fn, num_ceps, num_channels)
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
%   vad - VAD flag per frame from DSP
%   energy - weighted signal energy per frame from DSP
%   noise_energy - weighted noise floor energy per frame from DSP
%   frame_number - frame number from DSP

% SPDX-License-Identifier: BSD-3-Clause
% Copyright(c) 2022-2026 Intel Corporation. All rights reserved.

function [ceps, t, n, vad, energy, noise_energy, frame_number] = ...
	decode_ceps(fn, num_ceps, num_channels)

if nargin < 3
	num_channels = 1;
end

% MFCC stream
fs = 16e3;
qformat = 7;
magic = [25443 28006]; % ASCII 'mfcc' as int16
num_magic = 2; % magic word is 2 x int16

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

% Header after magic is [frame_number, reserved, energy, noise_energy, vad_flag]
% as int32 (10 int16 slots), followed by num_ceps coefficients.
payload_len = 10 + num_ceps; % 5 int32 = 10 int16, then ceps data

% Last frame can be incomplete due to span over multiple periods
last = idx(end) + num_magic + payload_len - 1;
if (last > length(data))
    num_frames = num_frames - 1;
end

t_ceps = period_ceps / num_channels / fs;
t = (0:num_frames -1) * t_ceps;
n = 1:num_ceps;

payload = zeros(payload_len, num_frames);
for i = 1:num_frames
	i1 = idx(i) + num_magic;
	i2 = i1 + payload_len - 1;
	payload(:,i) = double(data(i1:i2));
end

% Reassemble int32 from pairs of int16 (little-endian).
% Low half must be treated as unsigned with mod() to handle negative int16.
frame_number = mod(payload(1,:), 65536) + payload(2,:) * 65536;
% payload(3:4,:) is reserved, skip
energy = (mod(payload(5,:), 65536) + payload(6,:) * 65536) / 2^23;
noise_energy = (mod(payload(7,:), 65536) + payload(8,:) * 65536) / 2^23;
vad = mod(payload(9,:), 65536) + payload(10,:) * 65536;
ceps = payload(11:payload_len, :) / 2^qformat;

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
		if ~strcmp(t.class, 'int16')
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
