function test = fr_test_measure(test)

%% t = fr_test_measure(t)
%
% Measure frequency response from captured frequency sweep created by
% fr_test_input() that sets most of the input struct fields. The output
% is read from file t.fn_out.
%
% The test criteria is defined with first three or four latter input
% parameters. The latter has precedence if defined.
%
% Input parameters
% t.fr_lo        - Measure ripple start frequency
% t.fr_hi        - Measure ripple end frequency
% t.fr_rp_max_db - Max. ripple +/- dB
% t.fr_mask_flo  - Frequencies for lower mask
% t.fr_mask_fhi  - Frequencies for higher mask
% t.fr_mask_mlo  - Upper limits of mask
% t.fr_mask_mhi  - Lower limits of mask
%
% Output parameters
% t.f           - Frequency grid (Hz)
% t.m           - Measured frequency response (dB)
% t.rp          - Measured ripple +/- dB
% t.fr3db_hz    - Bandwidth in Hz for -3 dB attenuated response
% t.fail        - Returns zero for passed
% t.fh          - Figure handle
% t.ph          - Plot handle
%

% SPDX-License-Identifier: BSD-3-Clause
% Copyright(c) 2017 Intel Corporation. All rights reserved.
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

%% Check if upper and lower mask is defined
if length(test.fr_mask_flo) || length(test.fr_mask_fhi)
	test.fr_lo = 0;
	test.fr_hi = 0;
end
if test.fr_lo > 0 || test.fr_hi > 0
	test.fr_mask_flo = [];
	test.fr_mask_fhi = [];
	test.fr_mask_mlo = [];
	test.fr_mask_mhi = [];
end

test.ph = [];
test.fh = [];

%% Reference: AES17 6.2.3 Frequency response
%  http://www.aes.org/publications/standards/

[x, nx] = load_test_output(test);

default_result = NaN * ones(1,length(test.ch));
test.rp = default_result;
test.fr3db_hz = default_result;
test.fail = 0;
j = 1;
for channel = test.ch

        if nx == 0
                test.fail = 1;
                return
        end

        %% Find sync
        [d, nt, nt_use, nt_skip] = find_test_signal(x(:,j), test);
	if isempty(d)
		test.fail = 1;
		return
	end

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
	if test.fr_lo > 0 && test.fr_hi > 0 && ~isempty(test.fr_rp_max_db)
		idx0 = find(test.f < test.fr_hi);
		idx1 = find(test.f(idx0) > test.fr_lo);
		range_db = max(test.m(idx1,j))-min(test.m(idx1,j));
		test.rp(j) = range_db/2;
		if test.rp(j) > test.fr_rp_max_db
			fprintf('Failed response ch%d +/- %f dBpp (max +/- %f dB)\n', ...
				test.ch(j), test.rp(j), test.fr_rp_max_db);
			test.fail = 1;
		end
        end

        %% Find frequency response 3 dB 0-X Hz
        idx3 = find(test.m(:,j) > -3);
        test.fr3db_hz(j) = test.f(idx3(end));

        %% Interpolate mask in logaritmic frequencies, check
        if ~isempty(test.fr_mask_fhi)
                f_log = log(test.f);
                mask_hi = interp1(log(test.fr_mask_fhi), ...
				  test.fr_mask_mhi(:,j), f_log, 'linear');
                over_mask = test.m(:,j)-mask_hi';
                idx = find(isnan(over_mask) == 0);
                [m_over_mask, io] = max(over_mask(idx));
                if m_over_mask > 0
                        fprintf('Failed upper response mask around %.0f Hz\n', ...
                                test.f(io(1)));
                        test.fail = 1;
                end
        end
        if ~isempty(test.fr_mask_flo)
                f_log = log(test.f);
                mask_lo = interp1(log(test.fr_mask_flo), ...
				  test.fr_mask_mlo(:,j), f_log, 'linear');
                under_mask = mask_lo'-test.m(:,j);
                idx = find(isnan(under_mask) == 0);
                [m_under_mask, iu] = max(under_mask(idx));
                if m_under_mask > 0
                        fprintf('Failed lower response mask around %.0f Hz\n', ...
                                test.f(iu(1)));
                        test.fail = 1;
                end
        end

        %% Next channel to measure
        j=j+1;
end

%% Do plots

nr = j-1;
for i = 1:nr
	test.fh(i) = figure('visible', test.plot_visible);
	test.ph(i) = subplot(1,1,1);

	if test.plot_channels_combine
		semilogx(test.f, test.m);
	else
		semilogx(test.f, test.m(:,i));
	end

	grid on;
	xlabel('Frequency (Hz)');
	ylabel('Magnitude (dB)');
	if ~isempty(test.fr_mask_fhi)
		hold on;
		plot(test.f, mask_hi, 'k--');
		hold off;
	end
	if ~isempty(test.fr_mask_flo)
		hold on;
		plot(test.f, mask_lo, 'k--');
		hold off;
	end

	if ~isempty(test.plot_fr_axis)
		axis(test.plot_fr_axis);
	end

	if test.plot_channels_combine && (test.nch > 1)
		switch test.nch
			case 2
				lstr1 = sprintf("ch%d", test.ch(1));
				lstr2 = sprintf("ch%d", test.ch(2));
				legend(lstr1, lstr2, ...
					'Location', 'NorthEast');
		end
	else
		lstr = sprintf("ch%d", test.ch(i));
		legend(lstr, 'Location', 'NorthEast');
	end

	if test.plot_passband_zoom
		axes('Position', [ 0.2 0.2 0.4 0.2]);
		box on;
		i1 = find(test.f < test.fr_lo, 1, 'last');
		i2 = find(test.f > test.fr_hi, 1, 'first');
		if isempty(i1)
			i1 = 1;
		end
		if isempty(i2)
			i2 = length(test.f);
		end
		if test.plot_channels_combine
			plot(test.f(i1:i2), test.m(i1:i2,:));
		else
			plot(test.f(i1:i2), test.m(i1:i2,i));
		end
		rp = 1.0;
		if ~isempty(test.fr_rp_max_db)
			rp = test.fr_rp_max_db;
			hold on;
			plot([test.fr_lo test.fr_hi], [-rp/2 -rp/2], 'r--');
			plot([test.fr_lo test.fr_hi], [ rp/2  rp/2], 'r--');
			hold off
		end
		axis([0 test.f_max -rp rp]);
		grid on;
	end

	if test.plot_channels_combine
		break
	end

end

end
