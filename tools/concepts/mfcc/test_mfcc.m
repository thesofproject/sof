% Script to draft MFCC (Mel frequency cepstral coefficients) audio features
% calculating in SOF from audio stream.
%
% 1. Make STFT process with parameterized FFT size and hop. Hop is the number of
%    of samples between successive FFTs. FFT lengths should be 2^N to use SOF
%    library FFT.
% 2. Calculate power spectrum
% 3. Convert power spectrum to Mel bands
% 4. Convert linear power to Decibels
% 5. Convert each dB Mel vector to DCT (assume type-II)
% 6. The ceptral coefficiens are output at FFT hop rate. If SOF period is shorter
%    than hop, then every copy() does not trigger FFT. If period is longer than
%    hop several FFTs may be computed in a single copy().
%
% Some ideas are from Python code example of MFCC in
% https://www.kaggle.com/code/ilyamich/mfcc-implementation-and-tutorial/notebook

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2022, Intel Corporation. All rights reserved.

%% Main function

function [all_cc, t, n] = test_mfcc(fn, mfcc)

path(path(), '../../../test/cmocka/src/math/window');
path(path(), '../../../test/cmocka/src/math/auditory');
path(path(), '../../../test/cmocka/src/math/dct');

% Get input signal
stream.channels = 1;
stream.fs = 16e3;
stream.period = 1e-3;
stream.frames = round(stream.fs * stream.period);

% mfcc parameters, https://pytorch.org/audio/stable/compliance.kaldi.html#mfcc
if nargin  < 2
	mfcc = mfcc_defaults();
end

% Override defaults
if 0
	%mfcc.norm = 'slaney'; % Use norm or slaney
	mfcc.preemphasis_coefficient = 0;
	mfcc.remove_dc_offset = false;
	%mfcc.use_energy = true;
	%mfcc.window_type = 'rectangular'; % matches pytorch
	mfcc.window_type = 'hamming'; % matches pytorch
	%mfcc.window_type = 'hann'; % no match
	%mfcc.window_type = 'blackman'; % no match
	%mfcc.channel = 1; % Pick right channel
end

if 1
	mfcc.testvec = 1;
	mfcc.tv_ceps = 'ref_ceps.txt';
	mfcc.tv_mel = 'ref_fft_mel.txt';
	mfcc.tv_fft_out = 'ref_fft_out.txt';
	mfcc.tv_fft_in = 'ref_fft_out.in';
end

% Get audio file, extract desired channel
[x, stream.fs] = audioread(fn);
if mfcc.channel > -1
	x = x(:,mfcc.channel + 1);
end

% Test buffering with number sequnece
if 0
	sx = size(x);
	x = 1:sx(1);
end

% Initialize
state = init_mfcc(mfcc, stream);

% Simulate SOF stream
input_frames = (length(x) - stream.frames + 1);
all_mel_log = [];
all_cc = [];
idx = 1;
for i = 1:stream.frames:input_frames
	stream.period = x(i:i + stream.frames - 1);
	[mel_log, cc, state] = mfcc_copy(stream, mfcc, state);

	% Check if returned one of more Mel spectras
	if ~isempty(mel_log)
		s = size(mel_log);
		for j = 1:s(1)
			all_mel_log(idx, :) = mel_log;
			all_cc(idx, :) = cc;
			idx = idx + 1;
		end
	end
end

%size(all_mel_log)
%size(all_cc)

% Done, plot result
if 1
	figure
	s = size(all_mel_log);
	b = 1:s(2);
	n = (0:s(1)-1) * state.hop_size / stream.fs;
	surf(n, b, all_mel_log', 'EdgeColor', 'none');
	colormap(jet); view(0,90);
	xlabel('Time (s)');
	ylabel('Mel band')
end

figure
all_cc = transpose(all_cc);
s = size(all_cc);
n = 1:s(1);
t = (0:s(2)-1) * state.hop_size / stream.fs;
surf(t, n, all_cc, 'EdgeColor', 'none');
colormap(jet);
view(45, 60);
xlabel('Time (s)');
ylabel('Mel band cepstral coefficients')
title('Concept mfcc version');

test_close(mfcc, state);

end

%% Copy function

function [mel_log_matrix, cc_matrix, state] = mfcc_copy(stream, param, state)

mel_log_matrix = [];
cc_matrix = [];

% Copy from stream period to buffer
i1 = state.buffer_pos;
i2 = i1 + stream.frames - 1;
if i2 > state.buffer_size
	error('Buffer overflow');
end

% Pre-emphasis
if param.preemphasis_coefficient ~= 0
	[y, state.preemph_z] = filter(state.preemph_b, 1, stream.period, state.preemph_z);
	state.buffer(i1:i2) = y;
else
	state.buffer(i1:i2) = stream.period;
end

state.buffer_pos = i2 + 1;
n_filled = i2;

% Phase 1, wait until whole fft_size is filled with valid data. This way
% first output cepstral coefficients originate from streamed data and not
% from buffers with zero data.
if state.waiting_fill
	if n_filled < state.fft_size
		return
	else
		state.waiting_fill = 0;
	end
end

% Phase 2, move first prev_size data to previous data buffer, remove
% samples from input buffer.
if state.prev_samples_valid == 0
	n_move = state.prev_size;
	state.prev = state.buffer(1:n_move);
	n_remain = n_filled - n_move;
	state.buffer(1:n_remain) = state.buffer(n_move + 1:n_filled);
	n_filled = n_filled - n_move;
	state.buffer_pos = state.buffer_pos - n_move;
	state.prev_samples_valid = 1;
end

if n_filled >= state.hop_size
	% How many hops
	m = floor(n_filled / state.hop_size);
	for i = 1:m
		% Data for fft_size is available
		ip = 1; % pad end (right)
		fft_buf = zeros(state.fft_padded_size, 1);
		% Fill previous samples (prev_size)
		fft_buf(ip:ip + state.prev_size - 1) = state.prev;
		% New samples (hop_size)
		i3 = state.prev_size + ip;
		i4 = i3 + state.hop_size - 1;
		i5 = (i - 1) * state.hop_size + 1;
		i6 = i5 + state.hop_size - 1;
		fft_buf(i3:i4) = state.buffer(i5:i6);

		% Update prev
		p1 = ip + state.hop_size;
		p2 = p1 + state.prev_size - 1;
		state.prev = fft_buf(p1:p2);

		% Move unused samples to beginning of buffer, update position
		remain = n_filled - m * state.hop_size;
		if remain > 0
			state.buffer(1:remain) = state.buffer(i6 + 1:i6 + remain);
			state.buffer_pos = remain + 1;
		else
			state.buffer_pos = 1;
		end

		% Optional DC subtract, recommend to not do this since it
		% creates discontinuity to waveform
		if param.remove_dc_offset
			dc = mean(fft_buf(ip:end));
			fft_buf(ip:end) = fft_buf(ip:end) - dc;
		end

		if param.use_energy == true && param.raw_energy == true
			ie = ip + state.fft_padded_size - 1;
			log_energy = get_log_energy(fft_buf(ip:ie), param);
		end

		%% Window function
		fft_buf = do_win(state, fft_buf, ip);

		if param.use_energy == true && param.raw_energy == false
			ie = ip + state.fft_padded_size - 1;
			log_energy = get_log_energy(fft_buf(ip:ie), param);
		end

		%% Scale FFT input
		max_fft_buf = max(abs(fft_buf));
		input_shift = -ceil(log2(max_fft_buf));
		fft_buf = fft_buf * 2^input_shift;

		%% FFT
		scale_shift = log2(state.fft_padded_size);
		s_half = do_fft(state, fft_buf);
		testvec_print_complex(param, state.fh_fft_out, s_half);

		%% Mel spectrum
		mel_shift = input_shift - scale_shift;
		mel_log = mfcc_power_to_mel_log(state, param, s_half, mel_shift);
		testvec_print(param, state.fh_mel, mel_log);
		mel_log_matrix(i, :) = mel_log;

		%% DCT
		if 0
			cc0 = dct(mel_log); % DCT type-II (default)
			%cc0 = dct(mel_db, 'Type', 3); % DCT type-III
			cc = cc0(1:param.num_ceps);
		end
		if 1
			cc = mel_log * state.dct_matrix;
		end

		%% Lifter etc.
		if param.cepstral_lifter ~= 0
			cc = cc .* state.lifter_coeffs;
		end
		if param.use_energy == true
			cc(1) = log_energy;
		end
		cc_matrix(i, :) = cc;

		testvec_print(param, state.fh_ceps, cc);
	end
end

end

%% Initialize

function state = init_mfcc(param, stream)

if stream.fs ~= param.sample_frequency
	error('Stream rate does not match with parameters');
end

% Check non-supported options
if param.dither ~= 0
	error('Dither is not supported');
end

if param.htk_compat ~= false
	error('htk_compat is not supported');
end

if param.round_to_power_of_two ~= true
	error('Only power of two FFT size is supported');
end

if param.subtract_mean ~= false
	error('subtract_mean is not supported');
end

if param.snip_edges ~= true
	error('only snip_edges true is supported');
end

if param.min_duration ~= 0
	error('min_duration is not supported');
end

if param.vtln_warp ~= 1.0
	error('vtln is not implemented');
end

state.fft_size = round(param.frame_length * param.sample_frequency * 1e-3); % ms
state.hop_size = round(param.frame_shift * param.sample_frequency * 1e-3); % ms
state.prev_size = state.fft_size - state.hop_size;
state.prev = zeros(state.prev_size, 1);
state.buffer_size = state.fft_size + stream.frames;
state.buffer = zeros(state.buffer_size, 1);
state.buffer_pos = 1;

% TODO: Add needed windows, hanning is just a guess to get started
switch lower(param.window_type)
	case 'blackman'
		state.win = mfcc_blackman(state.fft_size, param.blackman_coef);
	case 'hamming'
		state.win = hamming(state.fft_size);
	case 'hann'
		state.win = hann(state.fft_size);
	case 'povey'
		state.win = mfcc_povey(state.fft_size);
	case 'rectangular'
		state.win = ones(state.fft_size, 1);

	otherwise
		error('Non-supported window');
end

% Zero pad FFT to next 2^N
state.fft_padded_size = 2^ceil(log2(state.fft_size));
state.zero_pad_fft = state.fft_padded_size - state.fft_size;
state.half_fft_size = state.fft_padded_size / 2 + 1;

% Get the Mel triangles
state = mfcc_get_mel_filterbank(param, state);

% Initialize DCT type-II
state.dct_matrix = mfcc_get_dct_matrix(param.num_ceps, param.num_mel_bins, 2);

if param.cepstral_lifter ~= 0
	state.lifter_coeffs = get_cepstral_lifter(param.cepstral_lifter, param.num_ceps);
end

% Initialize pre-emphasis filter, it's a 1st order FIR
% with coefficients b = [1 -preemphasis_coefficient].
state.preemph_z = [0];
state.preemph_b = [1 -param.preemphasis_coefficient];

% Initialize copy state
state.waiting_fill = 1; % Wait until complete fft_size is available
state.prev_samples_valid = 0; % Prev_samples is not yet valid.

% Test vectors
if param.testvec
	state.fh_ceps = fopen(param.tv_ceps, 'w');
	state.fh_mel = fopen(param.tv_mel, 'w');
	state.fh_fft_out = fopen(param.tv_fft_out, 'w');
	state.fh_fft_in = fopen(param.tv_fft_in, 'w');
end

end

%% Helper functions

function test_close(param, state)

if param.testvec
	fclose(state.fh_ceps);
	fclose(state.fh_mel);
	fclose(state.fh_fft_out);
	fclose(state.fh_fft_in);
end

end

function testvec_print(param, fh, val)

if param.testvec
	for i = 1:length(val)
		fprintf(fh, '%16.10e\n', val(i));
	end
end

end

function testvec_print_complex(param, fh, val)

if param.testvec
	for i = 1:length(val)
		fprintf(fh, '%16.10f %16.10f\n', real(val(i)), imag(val(i)));
	end
end

end

function log_energy = get_log_energy(x, param)

log_energy = log(max(sum(x .* x), eps));
if param.energy_floor > 0
	log_energy = max(log_energy, log(param.energy_floor));
end

end


function x = do_win(state, x, ip)

% Window after possible zero pad
n = length(state.win);
x(ip:ip + n - 1) = x(ip:ip + n - 1) .* state.win;

end

function s_half = do_fft(state, x)

% Return 0 .. Nyquist freqiency bins from FFT
scale = 1 / length(x);
s = fft(x);
s_half = s(1:state.half_fft_size) * scale;

end

% Ref: Kaldi mel-computation.c ComputeLifterCoeffs()
function lifter = get_cepstral_lifter(q, num_ceps)

i = 0:(num_ceps - 1);
lifter = 1.0 + 0.5 * q * sin(pi * i / q);

end
