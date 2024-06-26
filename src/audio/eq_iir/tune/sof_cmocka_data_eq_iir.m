% Create a chirp waveform and export test EQ coefficients and reference output
%
% Usage:
% cmocka_data_eq_iir()

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2021, Intel Corporation. All rights reserved.

function sof_cmocka_data_eq_iir()

% Output files and paths
sof_cmocka = '../../../../test/cmocka';
chirp_fn = fullfile(sof_cmocka, 'include/cmocka_chirp_2ch.h');
ref_fn = fullfile(sof_cmocka, 'src/audio/eq_iir/cmocka_chirp_iir_ref_2ch.h');
coef_fn = fullfile(sof_cmocka, 'src/audio/eq_iir/cmocka_iir_coef_2ch.h');

% Input data
fs = 48e3;
t = 100e-3;
scale = 2^31;
[x, yi] = get_chirp(fs, t);
sof_export_c_int32t(chirp_fn, 'chirp_2ch', 'CHIRP_2CH_LENGTH',yi)

% Compute a test EQ
eq = test_response(coef_fn, 'iir_coef_2ch', fs);

% Filter input data
[b, a] = zp2tf(eq.p_z, eq.p_p, eq.p_k);
ref(:,1) = filter(b, a, x(:,1));
ref(:,2) = filter(b, a, x(:,2));
[b, a] = zp2tf(eq.p_z, eq.p_p, eq.p_k);
refi = scale_saturate(ref, scale);
sof_export_c_int32t(ref_fn, 'chirp_iir_ref_2ch', 'CHIRP_IIR_REF_2CH_LENGTH', refi)

figure;
plot(yi/scale)
grid on;

figure;
plot(ref)
grid on;

figure;
plot(refi / scale)
grid on;

end


function xi = scale_saturate(x, scale)

imax = scale - 1;
imin = -scale;
xi = round(scale * x);
xi = min(xi, imax);
xi = max(xi, imin);

end

function [x, yi] = get_chirp(fs, t_chirp)

channels = 2;
f0 = 100;
f1 = 20e3;
a = 1 + 1e-5; % Ensure max and min int values are produced
scale = 2^31;
imax = scale - 1;
imin = -scale;

n = round(fs * t_chirp);
t = (0:(n - 1)) / fs;
x(:, 1) = a * chirp(t, f0, t_chirp, f1, 'logarithmic', 0);
x(:, 2) = a * chirp(t, f0, t_chirp, f1, 'logarithmic', 90);
x = min(x, 1.0);
x = max(x, -1.0);
yi = scale_saturate(x, scale);

end

%% -------------------
%% EQ design functions
%% -------------------

function eq = test_response(fn, vn, fs)

%% Get defaults for equalizer design
eq = sof_eq_defaults();
eq.fs = fs;
eq.enable_iir = 1;
eq.iir_norm_type = 'peak';
eq.iir_norm_offs_db = 1;

%% Parametric EQ
eq.peq = [ ...
                 eq.PEQ_HP2   100  0   0 ; ...
		 eq.PEQ_LS2  1000 +6   0 ; ...
		 eq.PEQ_PN2  3000 +6 1.0 ; ...
		 eq.PEQ_LP2 15000  0   0 ; ...
         ];

%% Design EQ
eq = sof_eq_compute(eq);

%% Plot
sof_eq_plot(eq);

%% Quantize and pack filter coefficients plus shifts etc.
bq = sof_eq_iir_blob_quant(eq.p_z, eq.p_p, eq.p_k);

%% Build blob
channels_in_config = 2;  % Setup max 2 channels EQ
assign_response = [0 0]; % Same response for L and R
num_responses = 1;       % One response
bm = sof_eq_iir_blob_merge(channels_in_config, ...
		       num_responses, ...
		       assign_response, ...
		       bq);

%% Pack and write file
bp = sof_eq_iir_blob_pack(bm);
sof_export_c_eq_uint32t(fn, bp, vn, 0);

end
