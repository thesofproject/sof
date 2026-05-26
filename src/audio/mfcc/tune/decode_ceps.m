% [ceps, t, n, vad, energy, noise_energy, frame_number] = decode_ceps(fn, num_ceps, hop, num_channels)
%
% Input
%   fn - File with MFCC data in .raw or .wav format
%   num_ceps - number of cepstral coefficients per frame
%   hop - STFT hop in seconds, defaults to 10e-3 for 10 ms
%   num_channels - needed for .raw format, omit for .wav, default 1
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
	decode_ceps(fn, num_ceps, hop, num_channels)

if nargin < 3
	hop = 10e-3;
end
if nargin < 4
	num_channels = 1;
end

% MFCC stream
qformat = 23; % Q9.23 in int32
magic = int32(1835426659); % 0x6D666363 as int32
num_magic = 1; % magic word is 1 x int32

% Load output data (always int32)
[data, num_channels] = get_file(fn, num_channels);

idx = find(data == magic);

if isempty(idx)
	error('No magic value markers found from stream');
end

num_frames = length(idx);

% Header after magic is [frame_number, reserved, energy, noise_energy, vad_flag]
% as int32, followed by num_ceps coefficients (int32).
payload_len = 5 + num_ceps;

% Last frame can be incomplete due to span over multiple periods
last = idx(end) + num_magic + payload_len - 1;
if (last > length(data))
    num_frames = num_frames - 1;
end

payload = zeros(payload_len, num_frames);
for i = 1:num_frames
	i1 = idx(i) + num_magic;
	i2 = i1 + payload_len - 1;
	payload(:,i) = double(data(i1:i2));
end

frame_number = payload(1, :);
% payload(2,:) is reserved, skip
energy = payload(3, :) / 2^23;
noise_energy = payload(4, :) / 2^23;
vad = payload(5, :);
ceps = payload(6:payload_len, :) / 2^qformat;

% Fill gaps from DTX-suppressed VAD=0 frames to create continuous timeline.
% Missing frames are filled with the minimum ceps value found in the data.
first_frame = frame_number(1);
last_frame = frame_number(end);
total_frames = last_frame - first_frame + 1;
if total_frames > num_frames
	ceps_fill = min(ceps(:));
	ceps_full = ones(num_ceps, total_frames) * ceps_fill;
	vad_full = zeros(1, total_frames);
	energy_full = zeros(1, total_frames);
	noise_energy_full = zeros(1, total_frames);
	frame_number_full = first_frame:last_frame;
	has_data = false(1, total_frames);
	for i = 1:num_frames
		fi = frame_number(i) - first_frame + 1;
		ceps_full(:, fi) = ceps(:, i);
		vad_full(fi) = vad(i);
		energy_full(fi) = energy(i);
		noise_energy_full(fi) = noise_energy(i);
		has_data(fi) = true;
	end
	% Forward-fill gaps with last received values
	for fi = 2:total_frames
		if ~has_data(fi)
			ceps_full(:, fi) = ceps_full(:, fi - 1);
			energy_full(fi) = energy_full(fi - 1);
			noise_energy_full(fi) = noise_energy_full(fi - 1);
		end
	end
	ceps = ceps_full;
	vad = vad_full;
	energy = energy_full;
	noise_energy = noise_energy_full;
	frame_number = frame_number_full;
end

t = (frame_number - first_frame) * hop;
n = 1:num_ceps;

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
		data = fread(fh, 'int32');
		fclose(fh);
	case '.wav'
		tmp = audioread(fn, 'native');
		t = whos('tmp');
		if ~strcmp(t.class, 'int32')
			error('Expected 32-bit wav for int32 MFCC output format');
		end
		s = size(tmp);
		num_channels = s(2);
		if num_channels > 1
			data = int32(zeros(prod(s), 1));
			for i = 1:num_channels
				data(i:num_channels:end) = tmp(:, i);
			end
		end
	otherwise
		error('Unknown audio format');
end

end
