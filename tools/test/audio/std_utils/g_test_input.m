function test = g_test_input(test)

%% t = dr_test_input(t)
%
% Create tone data file for playback & record on real device or
% for algorithm simulation.
%
% Input parameters
% t.fs        - sample rate
% t.bits_in   - signal word length
% t.ch        - mix test signal to channel ch
% t.nch       - total number of channels in data
%
% Output parameters
% t.fn_in     - created input file name
% t.fn_out    - proposed output file name for captured output
% t.f         - test signal frequency
% t.tl        - tone length in seconds
% t.ts        - tone start time
% t.tr        - tone gain ramp length in seconds
% t.ti        - ignore time from tone start and end, must be ti > tr
% t.a         - tone amplitude (lin)
% t.a_db      - tone amplitude (dB)
% t.mark_t    - length of marker tone in seconds
% t.mark_a    - amplitude max of marker tone (lin)
% t.mark_a_db - amplitude max of marker tone (dB)
%

% SPDX-License-Identifier: BSD-3-Clause
% Copyright(c) 2017 Intel Corporation. All rights reserved.
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

%% Reference: AES17 6.2.2 Gain
%  http://www.aes.org/publications/standards/


if nargin < 1
        fprintf('Warning, using default parameters!\n');
        test.fs = 48e3; test.bits=32; test.ch=1; test.nch=1;
end

if test.ch == 0
        test.ch = 1+round(rand(1,1)*(test.nch-1)); % Test random channel 1..Nch
end

for ch = test.ch
    fprintf('Using parameters Fs=%.1f, bits_in=%d, ch=%d, Nch=%d\n', ...
        test.fs/1e3, test.bits_in, ch, test.nch);
end

id = floor(rand(1,1) * 1e6);
test.fn_in = sprintf('g_test_in_%d.%s', id, test.fmt);
test.fn_out = sprintf('g_test_out_%d.%s', id, test.fmt);
test.f = 997;


%% Tone sweep parameters
test.is = 20e-3; % Ignore signal from tone start
test.ie = 20e-3; % Ignore signal from tone end
test.tr = 10e-3; % Gain ramp time for tones
test.sm = 3; % Seek start marker from 3s from start
test.em = 3; % Seek end marker from 3s from end
test.mt = 0.3; % Error if marker positions delta is greater than 0.3s
test.tc = 25; % Min. 20 cycles of sine wave for a frequency
test.a_db = -20; % -20 dBFS level
test.a = 10^(test.a_db/20);
% 0.5 seconds tone, this will be adjusted to be integer number of samples
test.tl = 0.5;

%% Mix the input file for test and write output
test = mix_sweep(test);

end
