% plot_filterbank_test - A helper for debugging unit test auditory

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright(c) 2022 Intel Corporation. All rights reserved.

% Load unit test debug data
load ../../../../../build_ut/test/cmocka/src/math/auditory/mel_filterbank_16.txt
load ../../../../../build_ut/test/cmocka/src/math/auditory/mel_filterbank_32.txt

scale = 1/2^7; % Q8.7
ref16 = mel_filterbank_16(:,1) * scale;
tst16 = mel_filterbank_16(:,2) * scale;
ref32 = mel_filterbank_32(:,1) * scale;
tst32 = mel_filterbank_32(:,2) * scale;

figure(1)
subplot(2, 1, 1);
plot(ref16); hold on; plot(tst16); hold off;
grid on;
title('16 bit');
subplot(2, 1, 2);
plot(ref16 - tst16);
grid on;

figure(2)
subplot(2, 1, 1);
plot(ref32); hold on; plot(tst32); hold off;
grid on;
title('32 bit');
subplot(2, 1, 2);
plot(ref32 - tst32);
grid on;
