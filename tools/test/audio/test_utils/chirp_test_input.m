function test = chirp_test_input(test)

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

if nargin < 1
        fprintf('Warning, using default parameters!\n');
        test.fs = 48e3; test.bits=32; test.ch=1; test.nch=1;
end

if test.ch == 0
        test.ch = 1:test.nch; % Test all channels 1..Nch
end

fprintf('Using parameters Fs=%.1f, bits_in=%d', test.fs/1e3, test.bits_in);
fprintf(', ch=%d', test.ch );
fprintf(', Nch=%d\n', test.nch );

test.fn_in = sprintf('chirp_test_in.%s', test.fmt);
test.fn_out =  sprintf('chirp_test_out.%s', test.fmt);


%% Chirp parameters
test.a_db = -0.1; % Near full scale
test.a = 10^(test.a_db/20);
test.f_min = 20;
test.f_max = 0.99*test.fs/2;
test.cl = 2.0;


%% Parameters to find signal between markers
test.nf = 1;
test.na = 1;
test.sm = 3; % Seek start marker from 3s from start
test.em = 3; % Seek end marker from 3s from end
test.mt = 0.3; % Error if marker positions delta is greater than 0.3s
test.is = 0; % Ignore signal from tone start
test.ie = 0; % Ignore signal from tone end

%% Mix the input file for test and write output
test = mix_chirp(test);

end
