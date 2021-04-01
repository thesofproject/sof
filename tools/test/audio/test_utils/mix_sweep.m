function  test = mix_sweep(test)

% SPDX-License-Identifier: BSD-3-Clause
% Copyright(c) 2017 Intel Corporation. All rights reserved.
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

%% Adjust tone lengt to integer number of samples
test.nt = round(test.tl*test.fs); % Make number of samples per tone
test.tl = test.nt/test.fs;        % an integer by adjusting tl.
test.nf = length(test.f); % Number of frequencies
test.na = length(test.a); % Number of amplitudes

%% Test start and end marker tones
[h1, mark_start] = sync_chirp(test.fs, 'up');
[h2, mark_end] = sync_chirp(test.fs, 'down');
test.mark_t = mark_start.t;
test.mark_a = mark_start.a;
test.mark_a_db = mark_start.a_db;
test.ts = mark_start.t;

%% Idle time to start and end
test.idle_t = 0.5;
n_idle = round(test.fs*test.idle_t);
t_idle = n_idle/test.fs;
x = zeros(test.nf*test.na*test.nt +mark_start.n +mark_end.n +2*n_idle, ...
        test.nch, 'int32');

%% Add markers
idx1 = n_idle+1;
idx2 = length(x)-n_idle-mark_end.n;
for ch=test.ch
        x(idx1:idx1+mark_start.n-1, ch) = dither_and_quantize(h1, test.bits_in);
        x(idx2:idx2+mark_end.n-1, ch) = dither_and_quantize(h2, test.bits_in);
end

%% Dither also idle parts
x(1:n_idle) = dither_and_quantize(zeros(n_idle,1), test.bits_in);
x(end-n_idle+1:end) = dither_and_quantize(zeros(n_idle,1), test.bits_in);

%% Add discrete frequencies sweep
n_r = round(test.fs*test.tr);
win = ones(test.nt,1);
win(1:n_r) = linspace(0,1,n_r);
win(end-n_r+1:end) = linspace(1,0,n_r);
i0 = n_idle+mark_start.n+1;
nn = 1;
for m=1:test.na
        for n=1:test.nf
                i1 = i0 + (nn-1)*test.nt;
                i2 = i1+test.nt-1;
                nn = nn+1;
                if length(test.f) > 1
                        f = test.f(n);
                else
                        f = test.f;
                end
                if length(test.a) > 1
                        a = test.a(m);
                else
                        a = test.a;
                end
                fprintf('Mixing %.0f Hz %.1f dBFS sine ...\n', f, 20*log10(a));
                s = multitone(test.fs, f, a, test.tl);
                for ch=test.ch
                        x(i1:i2, ch) = dither_and_quantize(s.*win, test.bits_in);
                end
        end
end

sx = size(x);
test.signal = x;
test.length_n = sx(1);
test.length_t = sx(1) / test.fs;

%% Output
fprintf('Writing output data file %s...\n', test.fn_in);
write_test_data(x, test.fn_in, test.bits_in, test.fmt, test.fs);
fprintf('Done.\n')

end
