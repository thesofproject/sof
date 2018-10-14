function test = aip_test_input(test)

%% t = aap_test_input(t)
%
% Create frequency sweep data file for playback & record on real device or
% for algorithm simulation.
%
% Input parameters
% t.f_start   - start frequency
% t.f_end     - end frequency
% t.fs        - sample rate
% t.bits_in   - number of bits in signal
% t.ch        - mix test signal to channel ch
% t.nch       - total number of channels in data
%
% Output parameters
% t.fn_in     - created input file name
% t.fn_out    - proposed output file name for captured output
% t.f         - frequencies in sweep
% t.nf        - number of frequencies
% t.tl        - tone length in seconds
% t.ts        - tone start times
% t.tr        - tone gain ramp length in seconds
% t.ti        - ignore time from tone start and end, must be ti > tr
% t.a         - tone amplitude (lin)
% t.a_db      - tone amplitude (dB)
% t.mark_t    - length of marker tone in seconds
% t.mark_a    - amplitude max of marker tone (lin)
% t.mark_a_db - amplitude max of marker tone (dB)
%

%%
% Copyright (c) 2017, Intel Corporation
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

%% Reference: AES17 6.6.7 Attenuation of image products
%  http://www.aes.org/publications/standards/

if nargin < 1
        error('This function must be called with argument.');
end

if test.ch == 0
        test.ch = 1+round(rand(1,1)*(test.nch-1)); % Test random channel 1..Nch
end

fprintf('Using parameters Fend=%d Hz, Fs=%.1f kHz, bits_in=%d, ch=%d, nch=%d\n', ...
        test.f_end, test.fs, test.bits_in, test.ch, test.nch);

test.fn_in = sprintf('aip_test_in.%s', test.fmt);
test.fn_out = sprintf('aip_test_out.%s', test.fmt);
f_first = 997;
test.f_start = 20;
f_min = min(test.f_start, test.f_end);
f_max = max(test.f_start, test.f_end);
test.fu = f_max;
n_thirdoct = ceil(log(f_max/f_min)/log(2)*3); % max 1/3 octave step
steps = n_thirdoct;
test.f = [f_first logspace(log10(test.f_start), log10(test.f_end), steps) ];

%% Tone sweep parameters
test.is = 20e-3; % Ignore signal from tone start
test.ie = 20e-3; % Ignore signal from tone end
test.tr = 10e-3; % Gain ramp time for tones
test.sm = 3; % Seek start marker from 3s from start
test.em = 3; % Seek end marker from 3s from end
test.mt = 0.1; % Error if marker positions delta is greater than 0.1s
test.tl = 3;
test.a_db = -20;
test.a = 10^(test.a_db/20);

%% Mix the input file for test and write output
test = mix_sweep(test);

end
