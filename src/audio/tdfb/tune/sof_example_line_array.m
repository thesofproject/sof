function sof_example_line_array()

% example_line_array()
%
% Creates a number of line array configuration blobs for devices
% with 2 microphones with spacing of 50 mm and 68 mm
% with 4 microphones with spacing of 28 mm, 68 mm, and 78 mm

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2020, Intel Corporation. All rights reserved.
%
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

%% 2 mic arrays
az = -90:15:90;
close all; line_one_beam(48e3, 50e-3, az, 2, 64);
close all; line_one_beam(48e3, 68e-3, az, 2, 64);
close all; line_one_beam(48e3, 73.5e-3, az, 2, 64);
close all; line_one_beam(16e3, 50e-3, az, 2, 40);
close all; line_one_beam(16e3, 68e-3, az, 2, 40);
close all; line_one_beam(16e3, 73.5e-3, az, 2, 40);

%% 4 mic arrays
close all; line_one_beam(48e3, 28e-3, az, 4, 80);
close all; line_one_beam(48e3, 68e-3, az, 4, 112);
close all; line_one_beam(48e3, 78e-3, az, 4, 120);

close all; line_one_beam(16e3, 28e-3, az, 4, 40);
close all; line_one_beam(16e3, 68e-3, az, 4, 52);
close all; line_one_beam(16e3, 78e-3, az, 4, 60);

end

function line_one_beam(fs, d, az, n_mic, n_fir)
	% Get defaults
	bf = sof_bf_defaults();
	bf.input_channel_select = 0:(n_mic-1);      % Input all n channels to filters
	bf.output_channel_mix = 3 * ones(1, n_mic); % Mix all filters to channel 1 and 2
	bf.output_stream_mix = zeros(1, n_mic);     % Mix filters to stream 0
	bf.num_output_channels = 2;                 % Stereo
	bf.num_output_streams = 1;                  % One sink stream
	bf.array = 'line';                          % Calculate xyz coordinates for line
	bf.mic_n = n_mic;

	% Mix only filters 1 and N to left and right stereo, it  gives best
	% passive stereo effect for beam off mode
	bf.output_channel_mix_beam_off = [1 zeros(1, n_mic-2) 2];

	% From parameters
	bf.fs = fs;
	bf.mic_d = d;
	bf.steer_az = az;
	bf.steer_el = 0 * az; % all 0 deg

	if nargin > 4
		bf.fir_length = n_fir;
	end

	% Design
	bf = sof_bf_filenames_helper(bf);
	bf = sof_bf_design(bf);
	bf.export_note = 'Created with script sof_example_line_array.m';
	bf.export_howto = 'cd tools/tune/tdfb; octave --no-window-system sof_example_line_array.m';
	sof_bf_export(bf);
end
