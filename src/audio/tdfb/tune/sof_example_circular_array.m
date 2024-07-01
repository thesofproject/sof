function sof_example_circular_array()

% example_circular_array()
%
% Creates a circular array configuration blobs for devices
% with 6 microphones and radius 30 mm

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2021, Intel Corporation. All rights reserved.
%
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

% Circular array with one beam and one output channel

az = -180:30:150;
circular_one_beam(16e3, 100e-3, 8, 28, az);
circular_one_beam(48e3, 100e-3, 8, 64, az);

end

% Note: In the example all designs are made 2ch out for the test script
% in tools/test/audio/tdfb_test.m to work correctly. So in this one beam
% example there's a dual mono (stereo) output. If there would be no need
% to build two beams design with same array this could be one channel out.

function circular_one_beam(fs, r, n, fir_length, az);

% Get defaults
bf = sof_bf_defaults();
bf.input_channel_select = 0:(n-1);            % Input all n channels to filters
bf.output_channel_mix_beam_off = zeros(1, n); % For some stereo symmetry
bf.output_channel_mix_beam_off(2) = 1;        % Mic2 to channel 2^0
bf.output_channel_mix_beam_off(n) = 2;        % Mic2 to channel 2^1
bf.output_channel_mix = 3 * ones(1, n);       % Mix all filters to channel 1 and 2
bf.output_stream_mix = zeros(1, n);           % No support yet
bf.num_output_channels = 2;                   % Two channels
bf.num_output_streams = 1;                    % One sink stream
bf.array = 'circular';                        % Calculate xyz coordinates for circular
bf.mic_n = n;

% From parameters
bf.fs = fs;
bf.mic_r = r;
bf.steer_az = az;
bf.steer_el = 0 * az;
bf.fir_length = fir_length;

% Design
bf = sof_bf_filenames_helper(bf);
bf = sof_bf_design(bf);
bf.export_note = 'Created with script sof_example_circular_array.m';
bf.export_howto = 'cd tools/tune/tdfb; octave --no-window-system sof_example_circular_array.m';
sof_bf_export(bf);
end
