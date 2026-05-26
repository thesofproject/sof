% [mel, t, n, vad, energy, noise_energy, frame_number] = decode_mel(fn, num_mel, hop, num_channels)
%
% Input
%   fn - File with Mel data in .raw or .wav format
%   hop - STFT hop in seconds, defaults to 10e-3 for 10 ms
%   num_mel - number of Mel coefficients per frame
%   num_channels - needed for .raw format, omit for .wav, default 1
%
% Outputs
%   mel - Mel coefficients
%   t - time vector for plotting
%   n - mel 1..num_mel vector for plotting
%   vad - VAD flag per frame from DSP
%   energy - weighted signal energy per frame from DSP
%   noise_energy - weighted noise floor energy per frame from DSP
%   frame_number - frame number from DSP

% SPDX-License-Identifier: BSD-3-Clause
% Copyright(c) 2026 Intel Corporation.

function [mel, t, n, vad, energy, noise_energy, frame_number] = ...
	decode_mel(fn, num_mel, hop, num_channels)

if nargin < 3
	hop = 10e-3;
end
if nargin < 4
	num_channels = 1;
end

% MFCC stream
fs = 16e3;
qformat = 23; % Q9.23 in int32

magic = int32(1835426659); % 0x6D666363 as int32
num_magic = 1; % magic word is 1 x int32
num_other_header = 5; % frame_number, reserved, energy, noise, vad (all int32)

% Load output data (always int32)
[data, num_channels] = get_file(fn, num_channels);

if isempty(data)
	error('File %s is empty', fn);
end

idx = find(data == magic);

if isempty(idx)
	error('No magic value markers found from stream');
end

period_mel = idx(2)-idx(1);
num_frames = length(idx);

% Header after magic is [frame_number, reserved, energy, noise_energy, vad_flag]
% as int32, followed by num_mel coefficients.
% For s16 each int32 occupies 2 int16 slots.
payload_len = num_other_header + num_mel;

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
mel = payload(6:payload_len, :) / 2^qformat;

% Fill gaps from DTX-suppressed VAD=0 frames to create continuous timeline.
% Missing frames are filled with the minimum Mel value found in the data.
first_frame = frame_number(1);
last_frame = frame_number(end);
total_frames = last_frame - first_frame + 1;
if total_frames > num_frames
	mel_fill = min(mel(:));
	mel_full = ones(num_mel, total_frames) * mel_fill;
	vad_full = zeros(1, total_frames);
	energy_full = zeros(1, total_frames);
	noise_energy_full = zeros(1, total_frames);
	frame_number_full = first_frame:last_frame;
	has_data = false(1, total_frames);
	for i = 1:num_frames
		fi = frame_number(i) - first_frame + 1;
		mel_full(:, fi) = mel(:, i);
		vad_full(fi) = vad(i);
		energy_full(fi) = energy(i);
		noise_energy_full(fi) = noise_energy(i);
		has_data(fi) = true;
	end
	% Forward-fill gaps with last received values
	for fi = 2:total_frames
		if ~has_data(fi)
			mel_full(:, fi) = mel_full(:, fi - 1);
			energy_full(fi) = energy_full(fi - 1);
			noise_energy_full(fi) = noise_energy_full(fi - 1);
		end
	end
	mel = mel_full;
	vad = vad_full;
	energy = energy_full;
	noise_energy = noise_energy_full;
	frame_number = frame_number_full;
end

t = (frame_number - first_frame) * hop;
n = 1:num_mel;

figure
imagesc(t, n, mel);
axis xy;
colormap(jet);
colorbar;
tstr = sprintf('SOF MFCC Mel coefficients (%s)', fn);
title(tstr, 'Interpreter', 'None');
ylabel('Mel coef #');

figure
subplot(2,1,1);
plot(t, vad)
ax = axis();
axis([ax(1:2) -0.1 1.1]);
grid on;
title(tstr, 'Interpreter', 'None');
xlabel('Time (s)');
ylabel('VAD flag');

subplot(2,1,2);
plot(t, energy, t, noise_energy);
grid on;
legend('Energy', 'Noise Energy');
xlabel('Time (s)');
ylabel('Energy');

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
			data = zeros(prod(s), 1, t.class);
			for i = 1:num_channels
				data(i:num_channels:end) = tmp(:, i);
			end
		else
			data = tmp;
		end
	otherwise
		error('Unknown audio format');
end

end
