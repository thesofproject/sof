% Wrapper function for librosa mfcc

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2022, Intel Corporation. All rights reserved.

function [coeffs, t, n] = test_mfcc_librosa(fn)

[~, fs] = audioread(fn);
cmd = sprintf('/usr/bin/python3 test_mfcc_librosa.py %s', fn);
[status, result] = system(cmd);
if status ~= 0
	cmd
	error("Failed to run Python script test_mfcc_librosa.py");
end

coeffs = load('librosa.csv');

figure;
s = size(coeffs);
n = 1:s(1);
t = ((1:s(2)) - 1) * 160 / fs;
surf(t, n, coeffs, 'EdgeColor', 'none');
colormap(jet);
view(45,60);
xlabel('Time (s)');
ylabel('Mel band cepstral coefficients')
title('Reference with librosa');

end
