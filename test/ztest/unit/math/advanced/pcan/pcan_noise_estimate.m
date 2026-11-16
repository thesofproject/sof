% pcan_noise_estimate - Octave fixed-point model of PCAN temporal IIR noise smoothing
%
% SPDX-License-Identifier: BSD-3-Clause
% Copyright(c) 2026 Intel Corporation. All rights reserved.

function [estimate_out, signal_out] = pcan_noise_estimate(estimate_in, signal_in, even_smoothing, odd_smoothing, min_signal_remaining, smoothing_bits, coef_bits)
% Inputs:
%   estimate_in          - Previous noise estimate vector (uint32)
%   signal_in            - Current frame input energy vector (uint32)
%   even_smoothing       - Even-channel IIR smoothing factor in Q(coef_bits)
%   odd_smoothing        - Odd-channel IIR smoothing factor in Q(coef_bits)
%   min_signal_remaining - Minimum signal floor ratio in Q(coef_bits)
%   smoothing_bits       - Scale shift for input energy (default 10)
%   coef_bits            - Number of fractional bits in smoothing coefs (default 14)
%
% Outputs:
%   estimate_out         - Updated noise estimate vector (uint32)
%   signal_out           - Noise-reduced signal vector (uint32)

	if nargin < 3
		even_smoothing = 410; % ~0.025 in Q14
	end
	if nargin < 4
		odd_smoothing = 983; % ~0.06 in Q14
	end
	if nargin < 5
		min_signal_remaining = 819; % ~0.05 in Q14
	end
	if nargin < 6
		smoothing_bits = 10;
	end
	if nargin < 7
		coef_bits = 14;
	end

	num_channels = length(signal_in);
	estimate_out = zeros(num_channels, 1, 'uint32');
	signal_out = zeros(num_channels, 1, 'uint32');

	for i = 1:num_channels
		if mod(i - 1, 2) == 0
			smoothing = even_smoothing;
		else
			smoothing = odd_smoothing;
		end
		one_minus_coef = bitshift(1, coef_bits) - smoothing;

		sig_scaled = bitshift(uint64(uint32(bitshift(uint32(signal_in(i)), smoothing_bits))), 0);
		est_prev = uint64(estimate_in(i));
		est_new = bitshift((sig_scaled * uint64(smoothing)) + (est_prev * uint64(one_minus_coef)), -coef_bits);
		estimate_out(i) = uint32(est_new);

		est_sub = uint64(estimate_out(i));
		if est_sub > sig_scaled
			est_sub = sig_scaled;
		end

		flr = bitshift(uint64(signal_in(i)) * uint64(min_signal_remaining), -coef_bits);
		subtracted = bitshift(sig_scaled - est_sub, -smoothing_bits);
		if subtracted > flr
			signal_out(i) = uint32(subtracted);
		else
			signal_out(i) = uint32(flr);
		end
	end
end
