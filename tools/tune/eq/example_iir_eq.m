%% Design effect EQs and bundle them to parameter block

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2016-2020, Intel Corporation. All rights reserved.
%
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

function example_iir_eq()

%% Common definitions
fs = 48e3;
fn.cpath3 = '../../ctl/ipc3';
fn.cpath4 = '../../ctl/ipc4';
fn.tpath1 =  '../../topology/topology1/m4';
fn.tpath2 =  '../../topology/topology2/include/components/eqiir';
fn.priv = 'DEF_EQIIR_PRIV';

addpath ../common

%% -------------------
%% Example 1: Loudness
%% -------------------
fn.bin = 'eq_iir_loudness.blob';
fn.txt = 'eq_iir_loudness.txt';
fn.tplg1 = 'eq_iir_coef_loudness.m4';
fn.tplg2 = 'loudness.conf';
comment = 'Loudness effect, created with example_iir_eq.m';

%% Design IIR loudness equalizer
eq_loud = loudness_iir_eq(fs);

%% Define a passthru IIR EQ equalizer
[z_pass, p_pass, k_pass] = tf2zp([1 0 0],[1 0 0]);

%% Quantize and pack filter coefficients plus shifts etc.
bq_pass = eq_iir_blob_quant(z_pass, p_pass, k_pass);
bq_loud = eq_iir_blob_quant(eq_loud.p_z, eq_loud.p_p, eq_loud.p_k);

%% Build blob
channels_in_config = 4;      % Setup max 4 channels EQ
assign_response = [1 1 1 1]; % Switch to response #1
num_responses = 2;           % Two responses: pass, loud
bm = eq_iir_blob_merge(channels_in_config, ...
		       num_responses, ...
		       assign_response, ...
		       [bq_pass bq_loud]);

%% Pack and write file
eq_pack_export(bm, fn, comment)

%% ------------------------------------
%% Example 2: Bass boost
%% ------------------------------------
fn.bin = 'eq_iir_bassboost.blob';
fn.txt = 'eq_iir_bassboost.txt';
fn.tplg1 = 'eq_iir_coef_bassboost.m4';
fn.tplg2 = 'bassboost.conf';
comment = 'Bass boost, created with example_iir_eq.m';

%% Design IIR bass boost equalizer
eq_bass = bassboost_iir_eq(fs);

%% Quantize and pack filter coefficients plus shifts etc.
bq_bass = eq_iir_blob_quant(eq_bass.p_z, eq_bass.p_p, eq_bass.p_k);

%% Build blob
channels_in_config = 2;    % Setup max 2 channels EQ
assign_response = [0 0];   % Switch to response #0
num_responses = 1;         % One responses: bass
bm = eq_iir_blob_merge(channels_in_config, ...
		       num_responses, ...
		       assign_response, ...
		       bq_bass);

%% Pack and write file
eq_pack_export(bm, fn, comment)

%% ------------------------------------
%% Example 3: Band-pass
%% ------------------------------------
fn.bin = 'eq_iir_bandpass.blob';
fn.txt = 'eq_iir_bandpass.txt';
fn.tplg1 = 'eq_iir_coef_bandpass.m4';
fn.tplg2 = 'bandpass.conf';
comment = 'Band-pass, created with example_iir_eq.m';

%% Design IIR bass boost equalizer
eq_band = bandpass_iir_eq(fs);

%% Quantize and pack filter coefficients plus shifts etc.
bq_band = eq_iir_blob_quant(eq_band.p_z, eq_band.p_p, eq_band.p_k);

%% Build blob
channels_in_config = 2;      % Setup max 2 channels EQ
assign_response = [0 0];     % Switch to response #0
num_responses = 1;           % One responses: bandpass
bm = eq_iir_blob_merge(channels_in_config, ...
		       num_responses, ...
		       assign_response, ...
		       bq_band);

%% Pack and write file
eq_pack_export(bm, fn, comment)

%% -------------------
%% Example 4: Flat IIR
%% -------------------
fn.bin = 'eq_iir_flat.blob';
fn.txt = 'eq_iir_flat.txt';
fn.tplg1 = 'eq_iir_coef_flat.m4';
fn.tplg2 = 'flat.conf';
comment = 'Flat response, created with example_iir_eq.m';

%% Define a passthru IIR EQ equalizer
[z_pass, p_pass, k_pass] = tf2zp([1 0 0],[1 0 0]);

%% Quantize and pack filter coefficients plus shifts etc.
bq_pass = eq_iir_blob_quant(z_pass, p_pass, k_pass);

%% Build blob
channels_in_config = 2;    % Setup max 2 channels EQ
assign_response = [0 0];   % Switch to response #0
num_responses = 1;         % One responses: pass
bm = eq_iir_blob_merge(channels_in_config, ...
		       num_responses, ...
		       assign_response, ...
		       bq_pass);

%% Pack and write file
eq_pack_export(bm, fn, comment)

%% ---------------------------
%% Example 5: Pass-through IIR
%% ---------------------------
fn.bin = 'eq_iir_pass.blob';
fn.txt = 'eq_iir_pass.txt';
fn.tplg1 = 'eq_iir_coef_pass.m4';
fn.tplg2 = 'passthrough.conf';
comment = 'Pass-through, created with example_iir_eq.m';

%% Define a passthru IIR EQ equalizer
[z_pass, p_pass, k_pass] = tf2zp([1 0 0],[1 0 0]);

%% Quantize and pack filter coefficients plus shifts etc.
bq_pass = eq_iir_blob_quant(z_pass, p_pass, k_pass);

%% Build blob
channels_in_config = 2;    % Setup max 2 channels EQ
assign_response = [-1 -1]; % Switch to passthrough
num_responses = 1;         % One responses: pass
bm = eq_iir_blob_merge(channels_in_config, ...
		       num_responses, ...
		       assign_response, ...
		       bq_pass);

%% Pack and write file
eq_pack_export(bm, fn, comment)

%% ------------------------------------
%% Example 6: 20/30/40/50 Hz high-pass
%% ------------------------------------

fs_list = [16e3 48e3];
fc_list = [20 30 40 50 100];
g_list = [0 20];
for i = 1:length(fs_list)
	for j = 1:length(fc_list);
		for k = 1:length(g_list);
			fs = fs_list(i);
			fc = fc_list(j);
			g = g_list(k);
			fsk = round(fs/1e3);
			fn.tplg1 = sprintf('eq_iir_coef_highpass_%dhz_%ddb_%dkhz.m4', ...
				           fc, g, fsk);
			fn.tplg2 = sprintf('highpass_%dhz_%ddb_%dkhz.conf', fc, g, fsk);
			fn.txt = sprintf('eq_iir_highpass_%dhz_%ddb_%dkhz.txt', ...
					 fc, g, fsk);
			comment = sprintf('%d Hz second order high-pass, gain %d dB, created with example_iir_eq.m', ...
					  fc, g);
			fn.bin = sprintf('eq_iir_highpass_%dhz_%ddb_%dkhz.blob', fc, g, fsk);

			%% Design IIR high-pass
			eq_hp = hp_iir_eq(fs, fc, g);

			%% Quantize and pack filter coefficients plus shifts etc.
			bq_hp = eq_iir_blob_quant(eq_hp.p_z, eq_hp.p_p, eq_hp.p_k);

			%% Build blob
			channels_in_config = 2;    % Setup max 2 channels EQ
			assign_response = [0 0];   % Switch to response #0
			num_responses = 1;         % One responses: hp
			bm = eq_iir_blob_merge(channels_in_config, ...
					       num_responses, ...
					       assign_response, ...
					       bq_hp);

			%% Pack and write file
			eq_pack_export(bm, fn, comment)
		end
	end
end

%% ------------------------------------------------------------------
%% Example 7: Merge previous desigs to single blob for use as presets
%% ------------------------------------------------------------------

fn.bin = 'eq_iir_bundle.blob';
fn.txt = 'eq_iir_bundle.txt';
fn.tplg1 = 'eq_iir_bundle.m4';
fn.tplg2 = 'bundle.conf';
comment = 'Bundle of responses flat/loud/bass/band/high, created with example_iir_eq.m';

%% Build blob
channels_in_config = 2;     % Setup max 2 channels EQ
assign_response = [ 2 2 ];  % Switch to response id 2, bass boost
num_responses = 5;          % 5 responses: flat, loudness, bass boost, bandpass, highpass
bm = eq_iir_blob_merge(channels_in_config, ...
		       num_responses, ...
		       assign_response, ...
		       [bq_pass bq_loud bq_bass bq_band bq_hp]);

%% Pack and write file
eq_pack_export(bm, fn, comment)

%% ------------------------------------
%% Done.
%% ------------------------------------

rmpath ../common

end

%% -------------------
%% EQ design functions
%% -------------------

function eq = loudness_iir_eq(fs)

%% Derived from Fletcher-Munson curves for 80 and 60 phon
f = [ 20,21,22,24,25,27,28,30,32,34,36,38,40,43,45,48,51,54,57,60,64, ...
        68,72,76,81,85,90,96,102,108,114,121,128,136,144,153,162,171, ...
        182,192,204,216,229,243,257,273,289,306,324,344,364,386,409, ...
        434,460,487,516,547,580,614,651,690,731,775,821,870,922,977, ...
        1036,1098,1163,1233,1307,1385,1467,1555,1648,1747,1851,1962, ...
        2079,2203,2335,2474,2622,2779,2945,3121,3308,3505,3715,3937, ...
        4172,4421,4686,4966,5263,5577,5910,6264,6638,7035,7455,7901, ...
        8373,8873,9404,9966,10561,11193,11861,12570,13322,14118,14962, ...
        15856,16803,17808,18872,20000];

m = [ 0.00,-0.13,-0.27,-0.39,-0.52,-0.64,-0.77,-0.89,-1.02,-1.16,  ...
        -1.31,-1.46,-1.61,-1.76,-1.91,-2.07,-2.24,-2.43,-2.64,-2.85, ...
        -3.04,-3.21,-3.35,-3.48,-3.62,-3.78,-3.96,-4.16,-4.35,-4.54, ...
        -4.72,-4.90,-5.08,-5.26,-5.45,-5.64,-5.83,-6.02,-6.19,-6.37, ...
        -6.57,-6.77,-6.98,-7.19,-7.40,-7.58,-7.76,-7.92,-8.08,-8.25, ...
        -8.43,-8.60,-8.76,-8.92,-9.08,-9.23,-9.38,-9.54,-9.69,-9.84, ...
        -9.97,-10.09,-10.18,-10.26,-10.33,-10.38,-10.43,-10.48,-10.54, ...
        -10.61,-10.70,-10.78,-10.85,-10.91,-10.95,-10.98,-11.02, ...
        -11.05,-11.07,-11.10,-11.11,-11.11,-11.10,-11.10,-11.11, ...
        -11.14,-11.17,-11.20,-11.21,-11.22,-11.21,-11.20,-11.20, ...
        -11.21,-11.21,-11.20,-11.17,-11.11,-11.02,-10.91,-10.78, ...
        -10.63,-10.46,-10.25,-10.00,-9.72,-9.39,-9.02,-8.62,-8.19, ...
        -7.73,-7.25,-6.75,-6.25,-5.75,-5.28,-4.87,-4.54,-4.33,-4.30];

%% Get defaults for equalizer design
eq = eq_defaults();
eq.fs = fs;
eq.target_f = f;
eq.target_m_db = m;
eq.enable_iir = 1;
eq.norm_type = 'loudness';
eq.norm_offs_db = 0;

%% Manually setup low-shelf and high shelf parametric equalizers
%
% Parametric EQs are PEQ_HP1, PEQ_HP2, PEQ_LP1, PEQ_LP2, PEQ_LS1,
% PEQ_LS2, PEQ_HS1, PEQ_HS2 = 8, PEQ_PN2, PEQ_LP4, and  PEQ_HP4.
%
% Parametric EQs take as second argument the cutoff frequency in Hz
% and as second argument a dB value (or NaN when not applicable) . The
% Third argument is a Q-value (or NaN when not applicable).
eq.peq = [ ...
                 eq.PEQ_LS1 40 +2 NaN ; ...
                 eq.PEQ_LS1 80 +3 NaN ; ...
                 eq.PEQ_LS1 200 +3 NaN ; ...
                 eq.PEQ_LS1 400 +3 NaN ; ...
                 eq.PEQ_HS2 13000 +7 NaN ; ...
         ];

%% Design EQ
eq = eq_compute(eq);

%% Plot
eq_plot(eq);

end

function eq = hp_iir_eq(fs, fc, gain_db)

% Get defaults for equalizer design
eq = eq_defaults();
eq.fs = fs;
eq.enable_iir = 1;
eq.norm_type = '1k';
eq.norm_offs_db = gain_db;

% Design
eq.peq = [ eq.PEQ_HP2 fc NaN NaN ];
eq = eq_compute(eq);
eq_plot(eq);

end

function eq = bassboost_iir_eq(fs)

% Get defaults for equalizer design
eq = eq_defaults();
eq.fs = fs;
eq.enable_iir = 1;
eq.norm_type = 'loudness';
eq.norm_offs_db = 0;

% Design
eq.peq = [ ...
                 eq.PEQ_HP2 30 NaN NaN ; ...
                 eq.PEQ_LS2 200 +10 NaN ; ...
         ];
eq = eq_compute(eq);
eq_plot(eq);

end

function eq = bandpass_iir_eq(fs)

% Get defaults for equalizer design
eq = eq_defaults();
eq.fs = fs;
eq.enable_iir = 1;
eq.norm_type = 'loudness';
eq.norm_offs_db = 0;

% Design EQ
eq.peq = [ ...
                 eq.PEQ_HP2 500 NaN NaN ; ...
                 eq.PEQ_LP2 5000 NaN NaN ; ...
         ];
eq = eq_compute(eq);
eq_plot(eq);

end

% Pack and write file common function for all exports
function eq_pack_export(bm, fn, note)

bp = eq_iir_blob_pack(bm, 3); % IPC3
if ~isempty(fn.bin)
	sof_ucm_blob_write(fullfile(fn.cpath3, fn.bin), bp);
end
if ~isempty(fn.txt)
	eq_alsactl_write(fullfile(fn.cpath3, fn.txt), bp);
end
if ~isempty(fn.tplg1)
	eq_tplg_write(fullfile(fn.tpath1, fn.tplg1), bp, fn.priv, note);
end

bp = eq_iir_blob_pack(bm, 4); % IPC4
if ~isempty(fn.bin)
	sof_ucm_blob_write(fullfile(fn.cpath4, fn.bin), bp);
end
if ~isempty(fn.txt)
	eq_alsactl_write(fullfile(fn.cpath4, fn.txt), bp);
end
if ~isempty(fn.tplg2)
	eq_tplg2_write(fullfile(fn.tpath2, fn.tplg2), bp, 'iir_eq', note);
end

end
