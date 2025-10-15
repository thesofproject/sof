% Get default parameters,
% Similar as https://pytorch.org/audio/stable/compliance.kaldi.html#mfcc

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2022, Intel Corporation. All rights reserved.

function mfcc = mfcc_defaults()

mfcc.blackman_coef = 0.42;
mfcc.cepstral_lifter = 22.0;
mfcc.channel = -1; % -1 expect mono, 0 left, 1 right ...
mfcc.dither = 0.0; % no support
mfcc.energy_floor = 1.0;
mfcc.frame_length = 25.0; % ms
mfcc.frame_shift = 10.0; % ms
mfcc.high_freq = 0.0; % 0 is Nyquist
mfcc.htk_compat = false; % no support
mfcc.low_freq = 20.0; % Hz
mfcc.num_ceps = 13;
mfcc.min_duration = 0.0; % no support
mfcc.norm_slaney = 0; % Use none or slaney
mfcc.num_mel_bins = 23;
mfcc.preemphasis_coefficient = 0.97;
mfcc.raw_energy = true;
mfcc.remove_dc_offset = true;
mfcc.round_to_power_of_two = true; % must be true
mfcc.sample_frequency = 16000;
mfcc.snip_edges = true; % must be true
mfcc.subtract_mean = false; % must be false
mfcc.use_energy = false;
mfcc.vtln_high = -500.0; % no support
mfcc.vtln_low = 100.0; % no support
mfcc.vtln_warp = 1.0; % must be 1.0 (vocal tract length normalization)
mfcc.window_type = 'povey';
mfcc.mel_log = 'MEL_LOG'; % Set to 'MEL_DB' for librosa, set to 'MEL_LOG10' for matlab
mfcc.pmin = eps; % Set to 1e-10 for librosa
mfcc.top_db = 200; % Set to 80 for librosa
mfcc.testvec = 0; % No testvectors output

end
