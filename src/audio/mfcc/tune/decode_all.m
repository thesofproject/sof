% decode_all.m - Decode all MFCC and Mel raw output files from run_mfcc.sh
%
% SPDX-License-Identifier: BSD-3-Clause
% Copyright(c) 2026 Intel Corporation.

num_ceps = 13;
num_mel = 80;

% MFCC cepstral output files
ceps_files = {'mfcc_s16.raw', 'mfcc_s24.raw', 'mfcc_s32.raw'};

% Mel output files with corresponding format
mel_files = {'mel_s16.raw', 'mel_s24.raw', 'mel_s32.raw'};
mel_fmts  = {'s16',         's24',          's32'};

% Xtensa prefixed variants
xt_ceps_files = {'xt_mfcc_s16.raw', 'xt_mfcc_s24.raw', 'xt_mfcc_s32.raw'};
xt_mel_files  = {'xt_mel_s16.raw',  'xt_mel_s24.raw',  'xt_mel_s32.raw'};

all_ceps_files = [ceps_files, xt_ceps_files];
all_mel_files  = [mel_files, xt_mel_files];
all_mel_fmts   = [mel_fmts, mel_fmts];

for i = 1:length(all_ceps_files)
	fn = all_ceps_files{i};
	if exist(fn, 'file')
		fprintf('Decoding MFCC ceps: %s\n', fn);
		[ceps, t, n] = decode_ceps(fn, num_ceps);
	end
end

for i = 1:length(all_mel_files)
	fn = all_mel_files{i};
	fmt = all_mel_fmts{i};
	if exist(fn, 'file')
		fprintf('Decoding Mel: %s\n', fn);
		[mel, t, n] = decode_mel(fn, num_mel, fmt);
	end
end
