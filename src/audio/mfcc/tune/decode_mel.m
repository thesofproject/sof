% [mel, t, n, vad, energy, noise_energy, frame_number] = decode_mel(fn, num_mel, fmt, num_channels)
%
% Input
%   fn - File with Mel data in .raw or .wav format
%   num_mel - number of Mel coefficients per frame
%   fmt - format of the Mel data ('s16', 's24', 's32')
%   num_channels - needed for .raw format, omit for .wav, default 2
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
	decode_mel(fn, num_mel, fmt, num_channels)

if nargin < 3
	fmt = 's16';
end

if nargin < 4
	num_channels = 2;
end

% MFCC stream
fs = 16e3;

switch fmt
  case 's16'
    qformat = 7;
    magic = [25443 28006]; % ASCII 'mfcc' as two int16
    word_size_multiplier = 2;
  case 's24'
    qformat = 15;
    magic = int32(1835426659); % 0x6D666363 as int32
    word_size_multiplier = 1;
  case 's32'
    qformat = 23;
    magic = int32(1835426659); % 0x6D666363 as int32
    word_size_multiplier = 1;
    otherwise
    error("Use 's16', 's24', or 's32' as format.");
end

num_magic = word_size_multiplier; % magic word is 2 x int16 or 1 x int32
num_other_header = 5 *  word_size_multiplier; % frame_number, reserved, energy, noise, vad

% Load output data
[data, num_channels] = get_file(fn, num_channels, fmt);

if strcmp(fmt, 's16')
	idx1 = find(data == magic(1));
	idx = [];
	for i = 1:length(idx1)
		next_word = idx1(i) + 1;
		if next_word <= length(data)
			if data(next_word) == magic(2)
				idx = [idx idx1(i)];
			end
		end
	end
else
	idx = find(data == magic);
end

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

if strcmp(fmt, 's16')
	% Reassemble int32 from pairs of int16 (little-endian).
	% Low half must be treated as unsigned with mod() to handle negative int16.
	frame_number = mod(payload(1,:), 65536) + payload(2,:) * 65536;
	% payload(3:4,:) is reserved, skip
	energy = (mod(payload(5,:), 65536) + payload(6,:) * 65536) / 2^23;
	noise_energy = (mod(payload(7,:), 65536) + payload(8,:) * 65536) / 2^23;
	vad = mod(payload(9,:), 65536) + payload(10,:) * 65536;
	mel = payload(11:payload_len, :) / 2^qformat;
else
	frame_number = payload(1, :);
	% payload(2,:) is reserved, skip
	energy = payload(3, :) / 2^23;
	noise_energy = payload(4, :) / 2^23;
	vad = payload(5, :);
	mel = payload(6:payload_len, :) / 2^qformat;
end

t_mel = period_mel / num_channels / fs;
t = (0:num_frames -1) * t_mel;
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

function [data, num_channels] = get_file(fn, num_channels, fmt)

[~, ~, ext] = fileparts(fn);

switch fmt
	case 's16'
		read_fmt = 'int16';
	case {'s24', 's32'}
		read_fmt = 'int32';
	otherwise
		error("Use 's16', 's24', or 's32' as format.");
end

switch lower(ext)
	case '.raw'
		fh = fopen(fn, 'r');
		data = fread(fh, read_fmt);
		fclose(fh);
	case '.wav'
		tmp = audioread(fn, 'native');
		t = whos('tmp');
		switch fmt
			case 's16'
				if ~strcmp(t.class, 'int16')
					error('Expected 16-bit wav for s16 format');
				end
			case {'s24', 's32'}
				if ~strcmp(t.class, 'int32')
					error('Expected 32-bit wav for %s format', fmt);
				end
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
