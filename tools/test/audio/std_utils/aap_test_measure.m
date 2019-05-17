function test = aap_test_measure(test)

% SPDX-License-Identifier: BSD-3-Clause
% Copyright(c) 2017 Intel Corporation. All rights reserved.
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

%% Reference: AES17 6.6.6 Attenuation of alias products
%  http://www.aes.org/publications/standards/

%% Load output file
[x, nx] = load_test_output(test);
if nx == 0
        test.fail = 1;
        return
end

%% Find sync
[d, nt, nt_use, nt_skip] = find_test_signal(x, test);

%% Measure
win = hamming(nt_use);
m0 = zeros(test.nf,1);
for n=1:test.nf
        fprintf('Measuring %.0f Hz ...\n', test.f(n));
        i1 = d+(n-1)*nt+nt_skip;
        i2 = i1+nt_use-1;
        m0(n) = level_dbfs(x(i1:i2).*win);
end

%% Calculate levels relative to first 997 Hz frequency,
% remove it from result, sort to ascinding order for plot
m_ref = m0(1);
test.m = m0(2:end)-m0(1);
[test.f, idx] = sort(test.f(2:end));
test.m = test.m(idx);
test.aap = max(test.m); % Worst-case

if test.aap > test.aap_max
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
