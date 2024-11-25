function sof_example_two_beams_default()

% sof_example_two_beams_default()
%
% Creates configuration files for a two beams design, one
% points to -10 degrees and other to 10 degrees
% direction for default 50 mm spaced two microphones configuration. The
% beams are output to stereo and left right channels. The angle is
% slightly different for different microphones spacing. With larger
% microphones spacing the angle narrows a bit. But since the target
% is user focused audio with slight stereo effect it's acceptable.
% The bespoke blobs for a device can be applied with ALSA UCM.

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2024, Intel Corporation.
%
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

%% Stereo capture blobs with two beams
az = [10];
azstr = az_to_string(az);

prm.export_note = 'Created with script sof_example_two_beams_default.m';
prm.export_howto = 'cd tools/tune/tdfb; matlab -nodisplay -nosplash -nodesktop -r sof_example_two_beams_default';
prm.type = 'DSB';
prm.add_beam_off = 1;

for fs = [16e3 48e3]
	%% Close all plots to avoid issues with large number of windows
	close all;

	%% 2 mic array
	fn.tplg1_fn = sprintf('coef_line2_generic_pm%sdeg_%dkhz.m4', azstr, fs/1e3);
	fn.sofctl3_fn = sprintf('coef_line2_generic_pm%sdeg_%dkhz.txt', azstr, fs/1e3);
	fn.tplg2_fn = sprintf('line2_generic_pm%sdeg_%dkhz.conf', azstr, fs/1e3);
	fn.sofctl4_fn = sprintf('line2_generic_pm%sdeg_%dkhz.txt', azstr, fs/1e3);
	d = 50e-3;  % 50 mm spacing
	a1 = az;   % Azimuth +az deg
	a2 = -az;  % Azimuth -az deg
	sof_bf_line2_two_beams(fs, d, a1, a2, fn, prm);

	%% 4 mic array
	fn.tplg1_fn = sprintf('coef_line4_generic_pm%sdeg_%dkhz.m4', azstr, fs/1e3);
	fn.sofctl3_fn = sprintf('coef_line4_generic_pm%sdeg_%dkhz.txt', azstr, fs/1e3);
	fn.tplg2_fn = sprintf('line4_generic_pm%sdeg_%dkhz.conf', azstr, fs/1e3);
	fn.sofctl4_fn = sprintf('line4_generic_pm%sdeg_%dkhz.txt', azstr, fs/1e3);
	d = 40e-3; % 40 mm spacing
	a1 = az;   % Azimuth +az deg
	a2 = -az;  % Azimuth -az deg
	sof_bf_line4_two_beams(fs, d, a1, a2, fn, prm);
end

end

function s = az_to_string(az)
	s = sprintf('%d', az(1));
	for n = 2:length(az)
		s = sprintf('%s_%d', s, az(n));
	end
end
