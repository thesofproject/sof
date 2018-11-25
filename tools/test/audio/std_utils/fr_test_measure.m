function test = fr_test_measure(test)

%% t = fr_test_measure(t)
%
% Measure frequency response from captured frequency sweep created by
% fr_test_input() that sets most of the input struct fiels. The output
% is read from file t.fn_out.
%
% Input parameters
% t.f_lo    - Measure ripple start frequency
% t.f_hi    - Measure ripple end frequency
% t.rp_max  - Max. ripple +/- dB
% t.mask_f  - Frequencies for mask
% t.mask_lo - Upper limits of mask
% t.mask_hi - Lower limits of mask
%
% Output parameters
% t.m           - Measured frequency response
% t.rp          - Measured ripple +/- dB
% t.fr3db_hz    - Bandwidth in Hz for -3 dB attenuated response
% t.fail        - Returns zero for passed
% t.fh          - Figure handle
% t.ph          - Plot handle
%
% E.g.
% t.fs=48e3; t.f_max=20e3; t.bits_out=16; t.ch=1; t.nch=2; t=fr_test_input(t);
% t.fn_out=t.fn_in; t.f_lo=20; t.f_hi=20e3; t.rp_max=[]; t.mask_f=[];
% t=fr_test_measure(t);
%

%%
% Copyright (c) 2017, Intel Corporation
% All rights reserved.
%
% Redistribution and use in source and binary forms, with or without
% modification, are permitted provided that the following conditions are met:
%   * Redistributions of source code must retain the above copyright
%     notice, this list of conditions and the following disclaimer.
%   * Redistributions in binary form must reproduce the above copyright
%     notice, this list of conditions and the following disclaimer in the
%     documentation and/or other materials provided with the distribution.
%   * Neither the name of the Intel Corporation nor the
%     names of its contributors may be used to endorse or promote products
%     derived from this software without specific prior written permission.
%
% THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
% AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
% IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
% ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
% LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
% CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
% SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
% INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
% CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
% ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
% POSSIBILITY OF SUCH DAMAGE.
%
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
%

%% Reference: AES17 6.2.3 Frequency response
%  http://www.aes.org/publications/standards/

[x, nx] = load_test_output(test);

j = 1;
for channel = test.ch

        if nx == 0
                test.rp(j) = NaN;
                test.fr3db_hz(j) = NaN;
                test.fail = 1;
                return
        end

        %% Find sync
        [d, nt, nt_use, nt_skip] = find_test_signal(x(:,j), test);

        win = hamming(nt_use);
        m0 = zeros(test.nf,1);
        for n=1:test.nf
                fprintf('Measuring %.0f Hz ...\n', test.f(n));
                i1 = d+(n-1)*nt+nt_skip;
                i2 = i1+nt_use-1;
                m0(n) = level_dbfs(x(i1:i2,j).*win) -test.a_db;
        end

        %% Scale 997 Hz point as 0 dB
        df = test.f - test.f_ref;
        [~,i997] = min(abs(df));
        m_offs = m0(i997);
        test.m(:,j) = m0 - m_offs;

        %% Check pass/fail
        idx0 = find(test.f < test.f_hi);
        idx1 = find(test.f(idx0) > test.f_lo);
        range_db = max(test.m(idx1,j))-min(test.m(idx1,j));
        test.rp(j) = range_db/2;
        test.fail = 0;
        if ~isempty(test.rp_max)
                if test.rp > test.rp_max
                        fprintf('Failed response +/- %f dBpp (max +/- %f dB)\n', ...
                                test.rp, test.rp_max);
                        test.fail = 1;
                end
        end

        %% Find frequency response 3 dB 0-X Hz
        idx3 = find(test.m(:,j) > -3);
        test.fr3db_hz(j) = test.f(idx3(end));

        %% Interpolate mask in logaritmic frequencies, check
        if ~isempty(test.mask_f)
                f_log = log(test.f);
                mask_hi = interp1(log(test.mask_f), test.mask_hi(:,j), ...
                        f_log, 'linear');
                mask_lo = interp1(log(test.mask_f), test.mask_lo(:,j), ...
                        f_log, 'linear');
                over_mask = test.m(:,j)-mask_hi';
                under_mask = mask_lo'-test.m(:,j);
                idx = find(isnan(over_mask) == 0);
                [m_over_mask, io] = max(over_mask(idx));
                idx = find(isnan(under_mask) == 0);
                [m_under_mask, iu] = max(under_mask(idx));
                if m_over_mask > 0
                        fprintf('Failed upper response mask around %.0f Hz\n', ...
                                test.f(io));
                        test.fail = 1;
                end
                if m_under_mask > 0
                        fprintf('Failed lower response mask around %.0f Hz\n', ...
                                test.f(iu));
                        test.fail = 1;
                end
        end

        test.fh(j) = figure('visible', test.visible);
        subplot(1,1,1);
        test.ph(j) = subplot(1,1,1);
        %semilogx(test.f, test.m(:,j));
        plot(test.f, test.m(:,j));
        grid on;
        xlabel('Frequency (Hz)');
        ylabel('Magnitude (dB)');
        if length(test.mask_f) > 0
                hold on;
                plot(test.f, mask_hi, 'r--');
                plot(test.f, mask_lo, 'r--');
                hold off;
        end
	ax = axis();
	axis([ax(1:2) -4 1]);
        if max(max(test.m(idx1,j))-min(test.m(idx1,j))) < 1
                axes('Position', [ 0.2 0.2 0.4 0.2]);
                box on;
                plot(test.f(idx1), test.m(idx1,j));
                if ~isempty(test.rp_max)
                        hold on;
                        plot([test.f_lo test.f_hi], ...
                                [-test.rp_max/2 -test.rp_max/2], 'r--');
                        plot([test.f_lo test.f_hi], ...
                                [ test.rp_max/2  test.rp_max/2], 'r--');
                        hold off;
                end
                grid on;
                axis([0 test.f_hi -0.1 0.1]);
        end

        %% Next channel to measure
        j=j+1;
end

end
