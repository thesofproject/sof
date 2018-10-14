function test = fr_test_input(test)

%% t = fr_test_input(t)
%
% Create frequency sweep data file for playback & record on real device or
% for algorithm simulation.
%
% Input parameters
% t.f_max     - maximum frequency of sweep, set e.g. to 0.99*fs/2
% t.fs        - sample rate
% t.bits_in   - number of bits in signal
% t.ch        - mix test signal to channel ch, e.g. set to [1 2] to measure
%               two channels
% t.nch       - total number of channels in data
%
% Output parameters
% t.fn_in     - Created input file name
% t.fn_out    - Proposed output file name for captured output
% t.f_ref     - Reference frequency used to report deviation (997 Hz)
% t.f_min     - Sweep start frequency
% t.f         - Frequencies in sweep
% t.is        - Ignore signal from tone start
% t.ie        - Ignore signal from tone end
% t.tr        - tone gain ramp length in seconds
% t.sm        - Seek start marker this time length from start
% t.em        - Seek end marker this time length from end
% t.mt        - Error if marker positions delta is greater than this
% t.tc        - Min cycles of sine wave per frequency
% t.tl        - Tone length in seconds
% t.a_db      - Tone amplitude (dB)
% t.a         - Tone amplitude (lin)
% t.nt        - Number of samples per tone
% t.nf        - Number of frequencies
% t.na        - Number of amplitudes
% t.mark_t    - Length of marker tone in seconds
% t.mark_a    - Amplitude max of marker tone (lin)
% t.mark_a_db - Amplitude max of marker tone (dB)
% t.ts        - Tone start times
%
% E.g.
% t.fs=48e3; t.f_max=20e3; t.bits_in=16; t.ch=1; t.nch=2; t = fr_test_input(t);
%

%%
% Copyright (c) 2016, Intel Corporation
% All rights reserved.
%
% Redistribution and use in source and binary forms, with or without
% modification, are permitted provided that the following conditions are met:
%   * Redistributions of source code must retain the above copyright
%     notice, this list of conditions and the following disclaimer.
%   * Redistributions in binary form must reproduce the above copyright
%     notice, this list of conditions and the following disclaimer in the
%     documentation and/or other materials provided with the distribution.
%   * Neither the name of the Intel Corporation nor the
%     names of its contributors may be used to endorse or promote products
%     derived from this software without specific prior written permission.
%
% THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
% AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
% IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
% ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
% LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
% CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
% SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
% INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
% CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
% ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
% POSSIBILITY OF SUCH DAMAGE.
%
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
%

%% Reference: AES17 6.2.3 Frequency response
%  http://www.aes.org/publications/standards/

if nargin < 1
        fprintf('Warning, using default parameters!\n');
        test.fs = 48e3; test.f_max = 0.99*test.fs/2; test.ch=1; test.nch=1;
        test.bits_in=32;
end

if test.ch == 0
        test.ch = 1+round(rand(1,1)*(test.nch-1)); % Test random channel 1..Nch
end

fprintf('Using parameters Fmax=%.1f kHz, Fs=%.1f, ch=%d, Nch=%d, bits_in=%d\n', ...
        test.f_max/1e3, test.fs/1e3, test.ch, test.nch, test.bits_in);

test.fn_in = sprintf('fr_test_in.%s', test.fmt);
test.fn_out =  sprintf('fr_test_out.%s', test.fmt);
test.f_ref = 997;
test.f_min = 20;

%% Use a dense frequency grid to see -3 dB point well
n_oct = ceil(log(test.f_max/test.f_ref)/log(2)*35);
f = logspace(log10(test.f_ref), log10(test.f_max), n_oct);
c = f(1)/f(2);
f_next = test.f_ref*c;
while (f_next > test.f_min)
        f = [f_next f];
        f_next = f_next*c;
end
test.f = f;

%% Tone sweep parameters
test.is = 20e-3; % Ignore signal from tone start
test.ie = 20e-3; % Ignore signal from tone end
test.tr = 10e-3; % Gain ramp time for tones
test.sm = 3; % Seek start marker from 3s from start
test.em = 3; % Seek end marker from 3s from end
test.mt = 0.1; % Error if marker positions delta is greater than 0.1s
test.tc = 10; % Min. 10 cycles of sine wave for a frequency
t_min = 0.2;
% Use t_min or min cycles count as tone length
test.tl = max(test.tc*1/min(f),t_min);
test.a_db = -20;
test.a = 10.^(test.a_db/20);

%% Mix the input file for test and write output
test = mix_sweep(test);

end
