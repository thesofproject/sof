% pcan_process - Octave model of full PCAN frame processing
%
% SPDX-License-Identifier: BSD-3-Clause
% Copyright(c) 2026 Intel Corporation. All rights reserved.

function [output_frames, final_noise_estimate] = pcan_process(input_frames, config, smoothing_bits, input_correction_bits)
% Inputs:
%   input_frames          - [num_channels x num_frames] matrix of uint32 filterbank energies
%   config.strength       - Exponent alpha (e.g. 0.95)
%   config.offset         - Additive constant delta (e.g. 80.0)
%   config.gain_bits      - Gain bit shift (e.g. 21)
%   config.smoothing_coef - Smoothing factor in Q14 (e.g. 819)
%   smoothing_bits        - Smoothing bit shift (e.g. 10)
%   input_correction_bits - Input correction shift (e.g. 0)
%
% Outputs:
%   output_frames         - [num_channels x num_frames] matrix of uint32 PCAN normalized outputs
%   final_noise_estimate  - [num_channels x 1] final noise estimate state

	if nargin < 3
		smoothing_bits = 10;
	end
	if nargin < 4
		input_correction_bits = 0;
	end

	[num_channels, num_frames] = size(input_frames);
	input_bits = smoothing_bits - input_correction_bits;
	kPcanSnrBits = 12;
	snr_shift = config.gain_bits - input_correction_bits - kPcanSnrBits;

	[gain_lut, ~] = pcan_gain_lookup(config, input_bits);

	noise_estimate = zeros(num_channels, 1, 'uint32');
	output_frames = zeros(num_channels, num_frames, 'uint32');

	for f = 1:num_frames
		sig_in = input_frames(:, f);

		% 1. Update temporal noise estimate
		noise_estimate = pcan_noise_estimate(noise_estimate, sig_in, config.smoothing_coef, smoothing_bits, 14);

		% 2. Apply PCAN gain control per channel
		frame_out = zeros(num_channels, 1, 'uint32');
		for c = 1:num_channels
			gain = pcan_wide_dynamic_func(noise_estimate(c), gain_lut);
			snr = bitshift(uint64(sig_in(c)) * uint64(uint16(gain)), -snr_shift);
			frame_out(c) = pcan_shrink(uint32(snr), 12, 6);
		end

		output_frames(:, f) = frame_out;
	end

	final_noise_estimate = noise_estimate;
end
