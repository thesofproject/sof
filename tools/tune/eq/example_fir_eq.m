%% Design demo EQs and bundle them to parameter block

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2016-2020, Intel Corporation. All rights reserved.
%
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

function example_fir_eq()

%% Common definitions
fs = 48e3;
fn.cpath3 = '../../ctl/ipc3';
fn.cpath4 = '../../ctl/ipc4';
fn.tpath1 =  '../../topology/topology1/m4';
fn.tpath2 =  '../../topology/topology2/include/components/eqfir';
fn.priv = 'DEF_EQFIR_PRIV';

addpath ../common

%% -------------------
%% Example 1: Loudness
%% -------------------
fn.bin = 'eq_fir_loudness.blob';
fn.txt = 'eq_fir_loudness.txt';
fn.tplg1 = 'eq_fir_coef_loudness.m4';
fn.tplg2 = 'loudness.conf';
comment = 'Loudness effect, created with example_fir_eq.m';

%% Design FIR loudness equalizer
eq_loud = loudness_fir_eq(fs);

%% Define a passthru EQ with one tap
b_pass = 1;

%% Quantize filter coefficients for both equalizers
bq_pass = eq_fir_blob_quant(b_pass);
bq_loud = eq_fir_blob_quant(eq_loud.b_fir);

%% Build blob
channels_in_config = 4;      % Setup max 4 channels EQ
assign_response = [1 1 1 1]; % Switch to response #1
num_responses = 2;           % Two responses pass, loud
bm = eq_fir_blob_merge(channels_in_config, ...
		       num_responses, ...
		       assign_response, ...
		       [ bq_pass bq_loud ]);

%% Pack and write file
eq_pack_export(bm, fn, comment);

%% -------------------
%% Example 2: Mid boost
%% -------------------
fn.bin = 'eq_fir_mid.blob';
fn.txt = 'eq_fir_mid.txt';
fn.tplg1 = 'eq_fir_coef_mid.m4';
fn.tplg2 = 'midboost.conf';
comment = 'Mid boost, created with example_fir_eq.m';

%% Define mid frequencies boost EQ
eq_mid = midboost_fir_eq(fs);

%% Quantize filter coefficients for both equalizers
bq_pass = eq_fir_blob_quant(eq_mid.b_fir);

%% Build blob
channels_in_config = 2;    % Setup max 2 channels EQ
assign_response = [0 0];   % Switch to response #0
num_responses = 1;         % One response: pass
bm = eq_fir_blob_merge(channels_in_config, ...
		       num_responses, ...
		       assign_response, ...
		       bq_pass);

%% Pack and write file
eq_pack_export(bm, fn, comment);

%% -------------------
%% Example 3: Flat EQ
%% -------------------
fn.bin = 'eq_fir_flat.blob';
fn.txt = 'eq_fir_flat.txt';
fn.tplg1 = 'eq_fir_coef_flat.m4';
fn.tplg2 = 'flat.conf';
comment = 'Flat response, created with example_fir_eq.m';

%% Define a passthru EQ with one tap
b_pass = 1;

%% Quantize filter coefficients for both equalizers
bq_pass = eq_fir_blob_quant(b_pass);

%% Build blob
channels_in_config = 2;    % Setup max 2 channels EQ
assign_response = [0 0];   % Switch to response #0
num_responses = 1;         % One response: pass
bm = eq_fir_blob_merge(channels_in_config, ...
		       num_responses, ...
		       assign_response, ...
		       bq_pass);

%% Pack and write file
eq_pack_export(bm, fn, comment);

%% --------------------------
%% Example 4: Pass-through EQ
%% --------------------------
fn.bin = 'eq_fir_pass.blob';
fn.txt = 'eq_fir_pass.txt';
fn.tplg1 = 'eq_fir_coef_pass.m4';
fn.tplg2 = 'passthrough.conf';
comment = 'Pass-through response, created with example_fir_eq.m';

%% Define a passthru EQ with one tap
b_pass = 1;

%% Quantize filter coefficients for both equalizers
bq_pass = eq_fir_blob_quant(b_pass);

%% Build blob
channels_in_config = 2;    % Setup max 2 channels EQ
assign_response = [-1 -1]; % Switch to response #0
num_responses = 1;         % One response: pass
bm = eq_fir_blob_merge(channels_in_config, ...
		       num_responses, ...
		       assign_response, ...
		       bq_pass);

%% Pack and write file
eq_pack_export(bm, fn, comment);


%% --------------------------
%% Done.
%% --------------------------

rmpath ../common

end

%% -------------------
%% EQ design functions
%% -------------------

function eq = loudness_fir_eq(fs)

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

%% Design EQ
eq = eq_defaults();
eq.fs = fs;
eq.target_f = f;            % Set EQ frequency response target: frequencies Hz
eq.target_m_db = m;         % Set EQ frequency response target: magnitudues dB
eq.norm_type = 'loudness';  % Normalize criteria can be loudness/peak/1k
eq.norm_offs_db = 0;        % Offset in dB to normalize

eq.enable_fir = 1;          % By default both FIR and IIR disabled, enable one
eq.fir_beta = 4.0;          % Use with care, low value can corrupt
eq.fir_length = 250;        % Long filter (test large IPC messages)
eq.fir_autoband = 0;        % Select manually frequency limits
eq.fmin_fir = 100;          % Equalization starts from 100 Hz
eq.fmax_fir = 20e3;         % Equalization ends at 20 kHz
eq.fir_minph = 0;           % Check result carefully if 1 is used, 0 is safe
eq = eq_compute(eq);

%% Plot
eq_plot(eq);

end

function eq = midboost_fir_eq(fs)

eq = eq_defaults();

eq.parametric_target_response = [ ...
					eq.PEQ_LS2 1200 -12 NaN ; ...
					eq.PEQ_HS2 7000 -12 NaN ; ...
				];

%% Design EQ
eq.fs = fs;
eq.norm_type = 'peak'; % Can be loudness/peak/1k to select normalize criteria
eq.norm_offs_db = 0;   % E.g. -1 would leave 1 dB headroom if used with peak

eq.enable_fir = 1;     % By default both FIR and IIR disabled, enable one
eq.fir_beta = 3.5;     % Use with care, low value can corrupt
eq.fir_length = 39;    % At limit of xtensa-gcc build speed
eq.fir_autoband = 0;   % Select manually frequency limits
eq.fmin_fir = 100;     % Equalization starts from 100 Hz
eq.fmax_fir = 20e3;    % Equalization ends at 20 kHz
eq.fir_minph = 1;      % If no linear phase required can test with 1
eq = eq_compute(eq);

%% Plot
eq_plot(eq);

end

% Pack and write file common function for all exports
function eq_pack_export(bm, fn, note)

bp = eq_fir_blob_pack(bm, 3); % IPC3
if ~isempty(fn.bin)
	sof_ucm_blob_write(fullfile(fn.cpath3, fn.bin), bp);
end
if ~isempty(fn.txt)
	eq_alsactl_write(fullfile(fn.cpath3, fn.txt), bp);
end
if ~isempty(fn.tplg1)
	eq_tplg_write(fullfile(fn.tpath1, fn.tplg1), bp, fn.priv, note);
end

bp = eq_fir_blob_pack(bm, 4); % IPC4
if ~isempty(fn.bin)
	sof_ucm_blob_write(fullfile(fn.cpath4, fn.bin), bp);
end
if ~isempty(fn.txt)
	eq_alsactl_write(fullfile(fn.cpath4, fn.txt), bp);
end
if ~isempty(fn.tplg2)
	eq_tplg2_write(fullfile(fn.tpath2, fn.tplg2), bp, 'fir_eq', note);
end

end
