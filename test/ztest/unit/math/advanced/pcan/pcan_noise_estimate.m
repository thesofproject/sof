% pcan_noise_estimate - Octave fixed-point model of PCAN temporal IIR noise smoothing
%
% SPDX-License-Identifier: BSD-3-Clause
% Copyright(c) 2026 Intel Corporation. All rights reserved.

function [estimate_out] = pcan_noise_estimate(estimate_in, signal_in, smoothing_coef, smoothing_bits, coef_bits)
% Inputs:
%   estimate_in    - Previous noise estimate vector (uint32)
%   signal_in      - Current frame input energy vector (uint32)
%   smoothing_coef - IIR smoothing factor in Q(coef_bits) (default 819 for 0.05 in Q14)
%   smoothing_bits - Scale shift for input energy (default 10)
%   coef_bits      - Number of fractional bits in smoothing_coef (default 14)
%
% Output:
%   estimate_out   - Updated noise estimate vector (uint32)

	if nargin < 3
		smoothing_coef = 819; % ~0.05 in Q14 (16384 * 0.05 = 819.2)
	end
	if nargin < 4
		smoothing_bits = 10;
	end
	if nargin < 5
		coef_bits = 14;
	end

	one_minus_coef = bitshift(1, coef_bits) - smoothing_coef;
	num_channels = length(signal_in);
	estimate_out = zeros(num_channels, 1, 'uint32');

	for i = 1:num_channels
		sig_scaled = bitshift(uint64(signal_in(i)), smoothing_bits);
		est_prev = uint64(estimate_in(i));
		est_new = bitshift((sig_scaled * uint64(smoothing_coef)) + (est_prev * uint64(one_minus_coef)), -coef_bits);
		estimate_out(i) = uint32(est_new);
	end
end
