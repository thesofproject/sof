% success = dmic_init(prm)
%
% Create PDM microphones interface configuration

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2019, Intel Corporation. All rights reserved.
%
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

function success = dmic_init(prm)

hw.controllers = 4;
hw.bits_cic = 26;
hw.bits_fir_coef = 20;
hw.bits_fir_gain = 20;
hw.bits_fir_input = 22;
hw.bits_fir_output = 24;
hw.bits_fir_internal = 26;
hw.bits_gain_output = 22;
hw.fir_length_max = 250;
hw.cic_shift_right_range = [-8 4];
hw.fir_shift_right_range  = [0 8];
hw.fir_max_length = 250;
hw.version = 1;
hw.number_of_pdm_controllers = 2;
hw.ioclk = 19.2e6;
spec.scale = 0.95;
spec.linear_phase = 1;
spec.rp = 0.1;
spec.cp = 0.4375;
spec.cs = 0.5100;
spec.rs = 95;
success = 0;

if (prm.fifo_a_fs == 0) && (prm.fifo_b_fs == 0)
        fprintf(1,'Error: At least one FIFO needs non-zero Fs!\n');
        return;
end

%% Match modes
[a_clkdiv, a_mcic, a_mfir] = find_modes(prm, hw, prm.fifo_a_fs);
[b_clkdiv, b_mcic, b_mfir] = find_modes(prm, hw, prm.fifo_b_fs);
[common_clkdiv_list,common_mcic_list,a_mfir_list,b_mfir_list] = ...
        match_modes(a_clkdiv, a_mcic, a_mfir, b_clkdiv, b_mcic, b_mfir);

if isempty(common_clkdiv_list)
        error('No compatible settings were found!\n');
end

%% Set basic configuration data
cfg = get_cfg(prm, hw);

%% Done, print 1st modes combination
fprintf('Selected fifo_a_fs=%d, fifo_b_fs=%d: ', prm.fifo_a_fs, prm.fifo_b_fs);
cfg = select_mode(common_mcic_list, a_mfir_list, b_mfir_list, common_clkdiv_list, cfg);
cfg = get_cic_config(prm, cfg, hw);
cfg = get_fir_config(prm, cfg, hw, spec);

end

%% Functions

%% Get FIR filters
function cfg = get_fir_config(prm, cfg, hw, spec)

if prm.fifo_a_fs > 0
        [coef, shift] = get_fir(cfg.mfir_a, prm, cfg, hw, spec);

        [cfg.fir_coef_a, cfg.fir_shift_a, cfg.fir_length_a] = ...
                scale_fir_coef(coef, shift, spec.scale, cfg.remain_gain_to_fir, hw.bits_fir_coef);
        cfg.fir_gain_a = 1;
else
        cfg.fir_gain_a = 0;
        cfg.fir_coef_a = 0;
        cfg.fir_shift_a = 0;
        cfg.fir_length_a = 1;
end

if prm.fifo_b_fs > 0
        [coef, shift] = get_fir(cfg.mfir_b, prm, cfg, hw, spec);

        [cfg.fir_coef_b, cfg.fir_shift_b, cfg.fir_length_b] = ...
		scale_fir_coef(coef, shift, spec.scale, cfg.remain_gain_to_fir, hw.bits_fir_coef);
        cfg.fir_gain_b = 1;
else
        cfg.fir_gain_b = 0;
        cfg.fir_coef_b = 0;
        cfg.fir_shift_b = 0;
        cfg.fir_length_b = 1;
end

end

function [coefq, shiftq, len] = scale_fir_coef(coef32, shift32, hw_sens, add_gain, bits)

out_scale = 2^(bits-1);
q31_scale = 2^31;
coef = coef32/q31_scale * 2^(-shift32) * hw_sens * add_gain;
max_abs_coef = max(abs(coef));
scale = 0.9999/max_abs_coef;
shiftq = floor(log(scale)/log(2));
c = 2^shiftq;
coefq = round(out_scale * coef * c);
len = length(coefq);

end

% This function becomes table lookup in C
function [coef32, shift] = get_fir(mfir, prm, cfg, hw, spec)

fs_fir = cfg.mic_clk/cfg.mcic;
fs = fs_fir/mfir;
passhz = spec.cp * fs;
stophz = spec.cs * fs;
max_length = min(hw.fir_max_length, floor(hw.ioclk/fs/2-5));

[ coef, shift, bw, sb, rs, got_pb, got_sb, passed] = ...
	dmic_fir(spec.rp, spec.rs, passhz, stophz, cfg.mic_clk, fs_fir, max_length, spec.linear_phase);

if passed == 0
        fprintf(1, 'Warning: Filter specification was reduced.\n');
        if (got_pb > 1) || (got_sb > -60)
                error('The design is erroneous.');
        end
end

coef32 = round(2^31 * coef);
fir.length = length(coef32);
fir.coef = coef32;
fir.shift = shift;
cp = bw/fs;
cs = sb/fs;
fir.cp = cp;
fir.cs = cs;
fir.rp = spec.rp;
fir.rs = rs;
fir.m = mfir;
dmic_fir_export(fir, 'include');

end

%% Select one mode from possible combinations
function cfg = select_mode(common_mcic_list, a_mfir_list, b_mfir_list, common_clkdiv_list, cfg)

cfg.mcic = 0;
cfg.mfir_a = 0;
cfg.mfir_b = 0;
cfg.clk_div = 0;

% Order of preference for FIR decimation factors, prime numbers
% even if low value, are less preferable due to incompatibility
% with other FIR decimation factor.
mpref = [2 4 6 8 3 5 7 9 10 11 12 13 14 15];

% Find common mode with lowest FIR decimation ratio. If there are many
% select one with lowest mic clock rate. Lowest rates or highest dividers
% are in the end of list.
if length(common_mcic_list) > 0
	for mtry = mpref
		idx = find(a_mfir_list == mtry);
		if ~isempty(idx)
			n = idx(end);
			break;
		end
	end
        cfg.mcic = common_mcic_list(n);
        cfg.clk_div = common_clkdiv_list(n);
        if a_mfir_list(1) > 0
                cfg.mfir_a = a_mfir_list(n);
        end
        if b_mfir_list(1) > 0
                cfg.mfir_b = b_mfir_list(n);
        end
        fprintf(1, 'clk_div=%d, cic=%d, fir_a=%d, fir_b=%d\n', ...
		cfg.clk_div, cfg.mcic, cfg.mfir_a, cfg.mfir_b);
end

end

%% Compute CIC filter settings
function cfg = get_cic_config(prm, cfg, hw)

cfg.mic_clk = hw.ioclk/cfg.clk_div;
g_cic = cfg.mcic^5;
bitsneeded = floor(log(g_cic)/log(2)+1)+1;
cfg.cic_shift = bitsneeded - hw.bits_fir_input;

if hw.bits_cic < bitsneeded
        fprintf(1,'Error: Needed CIC word length is exceeded %d\n', bitsneeded);
end

if cfg.cic_shift < hw.cic_shift_right_range(1);
        fprintf(1,'Warning: Limited CIC shift right from %d', cfg.cic_shift);
        cfg.cic_shift = hw.cic_shift_right_range(1);
end

if cfg.cic_shift > hw.cic_shift_right_range(2);
        fprintf(1,'Error: Limited CIC shift right from %d', cfg.cic_shift);
        cfg.cic_shift = hw.cic_shift_right_range(2);
end

% Compute how much gain is left for FIR from scaling to full scale
cfg.remain_gain_to_fir = 2^(hw.bits_fir_input-1)/(g_cic/2^cfg.cic_shift);

end

%% Find modes those are possible and exist in setup database
function [clkdiv, mcic, mfir] = find_modes(prm, hw, pcm_fs)

if pcm_fs < 1
        clkdiv = 0;
        mcic = 0;
        mfir = 0;
        return;
end
osr_min = 50;
if pcm_fs > 48e3
        osr_min = floor(3.0e6 / pcm_fs);
end
mcic_min = 5;
mcic_max = 31; % 8 bits reg but CIC gain and 26 bits implementation limits this
mfir_min = 2;
mfir_max = 15;
clkdiv_min = ceil(hw.ioclk/prm.pdmclk_max);
clkdiv_max = floor(hw.ioclk/prm.pdmclk_min);
n = 1;
clkdiv = [];
mcic = [];
mfir = [];
% Highest to lowest PDM clock, prefer best quality in range
for clkdiv_test = clkdiv_min:clkdiv_max
        if clkdiv_test > 4 % Limitation in cAVS1.5-2.0
                c1 = floor(clkdiv_test/2);
                c2 = clkdiv_test - c1;
                du_min = 100*c1/clkdiv_test;
                du_max = 100*c2/clkdiv_test;
                pdmclk = hw.ioclk/clkdiv_test;
                osr = round(pdmclk/pcm_fs);
                % Lowest FIR decimation to highest, prefer low FIR
                % decimation ratios
                for mfir_test = mfir_min:osr
                        mcic_test = floor(osr/mfir_test);
                        if (abs(pcm_fs*mfir_test*mcic_test -pdmclk) < 1)  ...
                                        && (osr >= osr_min) ...
                                        && (mcic_test >= mcic_min) ...
                                        && (mcic_test <= mcic_max) ...
                                        && (mfir_test <= mfir_max) ...
                                        && (du_min >= prm.duty_min) ...
                                        && (du_max <= prm.duty_max)
                                sfir = sprintf('FIR %d x %d x %dHz', ...
                                        mcic_test, mfir_test, round(pcm_fs));
                                %fprintf('Found: %s\n',sfir);
                                clkdiv(n) = clkdiv_test;
                                mcic(n) = mcic_test;
                                mfir(n) = mfir_test;
                                n=n+1;
                        end
                end
        end
end
end


%% Match found modes for common clkdiv and mcic, a DMIC hardware constraint
function [common_clkdiv_list,common_mcic_list,a_mfir_list,b_mfir_list] = ...
        match_modes(a_clkdiv, a_mcic, a_mfir, b_clkdiv, b_mcic, b_mfir)

if b_clkdiv == 0
        common_clkdiv_list = a_clkdiv;
        common_mcic_list = a_mcic;
        a_mfir_list = a_mfir;
        b_mfir_list = 0;
        return;
end
common_clkdiv_list = [];
common_mcic_list = [];
a_mfir_list = [];
b_mfir_list = [];
n_list = 1;
for n=1:length(a_clkdiv)
        idx = find(b_clkdiv == a_clkdiv(n));
        for m=1:length(idx)
                if b_mcic(idx(m)) == a_mcic(n)
                        common_clkdiv = a_clkdiv(n);
                        common_mcic = a_mcic(n);
                        common_clkdiv_list(n_list) = common_clkdiv;
                        common_mcic_list(n_list) = common_mcic;
                        a_mfir_list(n_list) = a_mfir(n);
                        b_mfir_list(n_list) = b_mfir(idx(m));
                        fprintf('Option %d: div=%d, mcic=%d, mfira=%d, mfirb=%d\n', ...
                                n_list, common_clkdiv, common_mcic, ...
                                a_mfir_list(n_list), b_mfir_list(n_list));
                        n_list = n_list+1;
                end
        end
end
end

%% Misc blob details needed
function cfg = get_cfg(prm, hw)

% if FIFO A or B is disabled, append a dummy blob to keep modes matching
% code happy with identical A and B request
if (prm.fifo_a_fs == 0) && (prm.fifo_b_fs == 0)
        fprintf('Error: FIFO A or B need to be assigned a nonzero samplerate\n');
        return;
end

cfg.hw_version = hw.version;

cfg.fifo_a_fs = prm.fifo_a_fs;
cfg.fifo_b_fs = prm.fifo_b_fs;

cfg.pdm01_provided = (prm.pdm(1).enable_mic_a | prm.pdm(1).enable_mic_b) ...
        + (prm.pdm(2).enable_mic_a | prm.pdm(2).enable_mic_b)*2;
cfg.ch01_provided = (prm.fifo_a_fs > 0) + (prm.fifo_b_fs > 0)*2;

if prm.fifo_a_fs > 0
        cfg.a_nchannels = prm.pdm(1).enable_mic_a + prm.pdm(1).enable_mic_b ...
                + prm.pdm(2).enable_mic_a + prm.pdm(2).enable_mic_b;
else
        cfg.a_nchannels = 0;
end
if prm.fifo_b_fs > 0
        cfg.b_nchannels = prm.pdm(1).enable_mic_a + prm.pdm(1).enable_mic_b ...
                + prm.pdm(2).enable_mic_a + prm.pdm(2).enable_mic_b;
else
        cfg.b_nchannels = 0;
end

end
