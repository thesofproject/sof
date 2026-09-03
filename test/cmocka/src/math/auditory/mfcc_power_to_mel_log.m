% mel_log =  mfcc_power_to_mel_log(state, param, s_half)
%
% Inputs
%   state.triangles - the Mel band triangles to integrate power
%   state.log_scale - coefficient to output desired log scale
%   param.pmin - min linear power to avoid log() of zero
%   param.top_db - limit lowest mel log value distance from peak
%   s_half - first half (N/2 + 1) of complex FFT data
%   shift - apply 2^(2 * shift) gain to Mel spectra
%
% Output
%   mel_log - logarithmic Mel band spectra

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2022, Intel Corporation. All rights reserved.

function mel_log = mfcc_power_to_mel_log(state, param, s_half, shift)

p_lin = s_half .* conj(s_half);
mel_log = zeros(1, param.num_mel_bins);
for i = 1:param.num_mel_bins
	p = sum(p_lin .* state.triangles(:, i));
	p = max(p, param.pmin);
	% Compensate linear spectra shift to logarithmic Mel power spectra
	mel_log(i) = (log(p) - 2 * shift * log(2)) * state.log_scale;
end

if param.top_db > 0
	peak_db = max(mel_log);
	mel_log = max(mel_log, peak_db - param.top_db);
end

end
