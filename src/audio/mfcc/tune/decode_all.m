% decode_all.m - Decode all MFCC and Mel raw output files from run_mfcc.sh
%
% SPDX-License-Identifier: BSD-3-Clause
% Copyright(c) 2026 Intel Corporation.

num_ceps = 13;
num_mel = 80;

% MFCC cepstral output files (all int32 output, Q9.23)
ceps_files = {'mfcc_s16.raw', 'mfcc_s24.raw', 'mfcc_s32.raw'};

% Mel output files (all int32 output, Q9.23)
mel_files = {'mel_s16.raw', 'mel_s24.raw', 'mel_s32.raw'};

% Xtensa prefixed variants
xt_ceps_files = {'xt_mfcc_s16.raw', 'xt_mfcc_s24.raw', 'xt_mfcc_s32.raw'};
xt_mel_files  = {'xt_mel_s16.raw',  'xt_mel_s24.raw',  'xt_mel_s32.raw'};

all_ceps_files = [ceps_files, xt_ceps_files];
all_mel_files  = [mel_files, xt_mel_files];

for i = 1:length(all_ceps_files)
	fn = all_ceps_files{i};
	if exist(fn, 'file')
		fprintf('Decoding MFCC ceps: %s\n', fn);
		[ceps, t, n, vad, energy, noise_energy, frame_num] = ...
			decode_ceps(fn, num_ceps);
	end
end

for i = 1:length(all_mel_files)
	fn = all_mel_files{i};
	if exist(fn, 'file')
		fprintf('Decoding Mel: %s\n', fn);
		[mel, t, n, vad, energy, noise_energy, frame_num] = ...
			decode_mel(fn, num_mel);
	end
end
