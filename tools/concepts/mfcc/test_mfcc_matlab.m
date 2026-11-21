% Script to run MFCC with Matlab libraries

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2022, Intel Corporation. All rights reserved.

function [coeffs, t, n] = test_mfcc_matlab(fn)

if exist('OCTAVE_VERSION', 'builtin') ~= 0
	error("This script runs only in Matlab.");
end

% Get audio file, extract desired channel
[x, fs] = audioread(fn);

if 0
	% Gave up with this, don't know how to set BandEdges
	win = hann(1024, 'periodic');
	coeffs = mfcc(x, fs, 'Window', win, ...
		'OverlapLength', 512, ...
		'NumCoeffs', 13, ...
		'FFTLength', numel(win), ...
		'Rectification', 'log', ...
		'LogEnergy', 'Ignore');
	%'BandEdges', [], ...
	size(coeffs)
end

if 1
	% Based on example from doc cepstralCoefficients, changed
	% to power spectrum instead of abs(S)
	fftLength = 512;
	windowLength = 400;
	overlapLength = 400-160;
	numBands = 13;
	range = [0 fs/2];
	normalization = 'bandwidth';
	S = stft(x, "Window", hann(windowLength), 'FFTLength', fftLength, ...
		"OverlapLength", overlapLength, "FrequencyRange", "onesided");
	P = S .* conj(S);
	filterBank = designAuditoryFilterBank(fs,'FFTLength',fftLength, ...
		"NumBands",numBands, ...
		"FrequencyRange",range, ...
		"Normalization",normalization);
	melSpec = filterBank * P;
	coeffs = cepstralCoefficients(melSpec);
end

figure;
coeffs = transpose(coeffs);
s = size(coeffs);
n = 1:s(1);
t = ((1:s(2)) - 1) * 160 / fs;
surf(t, n, coeffs, 'EdgeColor', 'none');
colormap(jet);
view(45,60);
xlabel('Time (s)');
ylabel('Mel band cepstral coefficients')
title('Reference with Matlab functions');

end
