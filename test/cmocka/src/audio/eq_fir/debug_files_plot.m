% debug_files_plot() - plot optional debug output

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright(c) 2022 Intel Corporation. All rights reserved.

load ../../../../../build_ut/test/cmocka/src/audio/eq_fir/fir_test_16.txt;
iref = fir_test_16(:,1);
iout = fir_test_16(:,2);
ref = reshape(iref, size(iref, 1)/2, 2);
out = reshape(iout, size(iout, 1)/2, 2);
figure;
subplot(2,1,1);
plot(ref)
hold on
plot(out)
hold off
grid on
title('16 bit FIR');
subplot(2,1,2);
plot(ref - out);
grid on

load ../../../../../build_ut/test/cmocka/src/audio/eq_fir/fir_test_24.txt;
iref = fir_test_24(:,1);
iout = fir_test_24(:,2);
ref = reshape(iref, size(iref, 1)/2, 2);
out = reshape(iout, size(iout, 1)/2, 2);
figure;
subplot(2,1,1);
plot(ref)
hold on
plot(out)
hold off
grid on
title('24 bit FIR');
subplot(2,1,2);
plot(ref - out);
grid on

load ../../../../../build_ut/test/cmocka/src/audio/eq_fir/fir_test_32.txt;
iref = fir_test_32(:,1);
iout = fir_test_32(:,2);
ref = reshape(iref, size(iref, 1)/2, 2);
out = reshape(iout, size(iout, 1)/2, 2);
figure;
subplot(2,1,1);
plot(ref)
hold on
plot(out)
hold off
grid on
title('32 bit FIR');
subplot(2,1,2);
plot(ref - out);
grid on
