% Wrapper function for pytorch mfcc

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2022, Intel Corporation. All rights reserved.

function [coeffs, t, n] = test_mfcc_kaldi(fn)

[~, fs] = audioread(fn);
cmd = sprintf('python3 test_mfcc_kaldi.py %s', fn);
[status,result] = system(cmd);
if status ~= 0
	error("Failed to run Python script test_mfcc_kaldi.py");
end

coeffs = transpose(load('kaldi.csv'));

figure;
s = size(coeffs);
n = 1:s(1);
t = ((1:s(2)) - 1) * 160 / fs;
surf(t, n, coeffs, 'EdgeColor', 'none');
colormap(jet);
view(45,60);
xlabel('Time (s)');
ylabel('Mel band cepstral coefficients')
title('Reference with Pytorch Kaldi compliance function');

end
