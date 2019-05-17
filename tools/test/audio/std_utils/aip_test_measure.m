function test = aip_test_measure(test)

% SPDX-License-Identifier: BSD-3-Clause
% Copyright(c) 2019 Intel Corporation. All rights reserved.
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

%% Reference: AES17 6.6.7 Attenuation of image products
%  http://www.aes.org/publications/standards/

%% Load output file
[x, nx] = load_test_output(test);
if nx == 0
        test.g_db = NaN;
        test.fail = 1;
        return
end

%% Find sync
[d, nt, nt_use, nt_skip] = find_test_signal(x, test);

%% Measure all test frequencies
t_skip = 1.0;
ml = zeros(test.nf,1);
mn = zeros(test.nf,1);
b_lpf = stdlpf_get(test.fu, test.fs); % Get LPF coef
b_hpf = stdhpf_get(test.fu, test.fs); % Get HPF coef
for n=1:test.nf
        fprintf('Measuring %.0f Hz ...\n', test.f(n));
        % Get notch coef for this frequency
        [b_notch, a_notch] = stdnotch_get(test.f(n), test.fs);
        i1 = d+(n-1)*nt+nt_skip;
        i2 = i1+nt_use-1;
        x_lpf = filter(b_lpf, 1, x(i1:i2)); % Standard LPF, need for 997 Hz only
        x_notch = filter(b_notch, a_notch, x(i1:i2)); % Standard notch
        x_hpf = filter(b_hpf, 1, x_notch); % Standard HPF
        ml(n) = level_dbfs(x_lpf(round(t_skip*test.fs):end));
        mn(n) = level_dbfs(x_hpf(round(t_skip*test.fs):end));
end

%% Calculate levels relative to first 997 Hz frequency,
% remove it from result, sort to ascinding order for plot
test.f = test.f(2:end);
test.m = mn(2:end)-ml(1);
test.aip = max(test.m); % Worst-case
if test.aip > test.aip_max
        test.fail = 1;
else
        test.fail = 0;
end

test.fh = figure('visible', test.plot_visible);
semilogx(test.f, test.m);
grid on;
xlabel('Frequency (Hz)');
ylabel('Relative level (dB)');
grid on;

end
