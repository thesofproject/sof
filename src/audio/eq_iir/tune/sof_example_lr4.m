% sof_example_lr4 - Design 4th order Linkwitz–Riley filter bank
%
% This script is run without arguments. It creates IIR equalizer
% blobs for crossover filter bank for 2-way speaker and four
% channels stream. The exported configurations are Linkwitz-Riley
% 4th order with crossover frequency at 2 kHz. The filters are
% in order:
% - low, high, low, high
% - low, low, high, high
% - high, high, low, low
%

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2025, Intel Corporation.
%
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

function sof_example_lr4()

%% Common definitions
fs = 48e3;
fc = 2e3;
sof_tools = '../../../../tools';
tpath = fullfile(sof_tools, 'topology/topology2/include/components/eqiir');
cpath = fullfile(sof_tools, 'ctl/ipc4/eq_iir');

sof_eq_paths(1);

%% --------------------------------------------------
%% Example: Band-split 2ch to 4ch low and high bands
%% --------------------------------------------------
design_name = sprintf('xover_lr4_%dhz_lhlh_%dkhz', fc, round(fs/1000));
blob_fn = fullfile(cpath, [design_name '.bin']);
alsa_fn = fullfile(cpath, [design_name '.txt']);
tplg_fn = fullfile(tpath, [design_name '.conf']);
comment = 'LR4 filter bank coefficients';
howto = 'cd src/audio/eq_iir/tune; octave sof_example_lr4.m';

% Design low-pass and high-pass filters
eq_lo = lo_band_iir(fs, fc);
eq_hi = hi_band_iir(fs, fc);

% Quantize and pack filter coefficients plus shifts etc.
bq_lo = sof_eq_iir_blob_quant(eq_lo.p_z, eq_lo.p_p, eq_lo.p_k);
bq_hi = sof_eq_iir_blob_quant(eq_hi.p_z, eq_hi.p_p, eq_hi.p_k);

% Build blob
channels_in_config = 4;      % Setup max 4 channels EQ
assign_response = [0 1 0 1]; % Order: lo, hi, lo, hi
num_responses = 2;           % Two responses: lo, hi
bm = sof_eq_iir_blob_merge(channels_in_config, ...
		       num_responses, ...
		       assign_response, ...
		       [bq_lo bq_hi]);

% Pack and write file
sof_eq_pack_export(bm, blob_fn, alsa_fn, tplg_fn, comment, howto)

%% --------------------------------------------------
%% Example: Same but filters order is lo, lo, hi, hi
%% --------------------------------------------------

design_name = sprintf('xover_lr4_%dhz_llhh_%dkhz', fc, round(fs/1000));
blob_fn = fullfile(cpath, [design_name '.bin']);
alsa_fn = fullfile(cpath, [design_name '.txt']);
tplg_fn = fullfile(tpath, [design_name '.conf']);

assign_response = [0 0 1 1];
num_responses = 2;
bm = sof_eq_iir_blob_merge(channels_in_config, ...
		       num_responses, ...
		       assign_response, ...
		       [bq_lo bq_hi]);

% Pack and write file
sof_eq_pack_export(bm, blob_fn, alsa_fn, tplg_fn, comment, howto)

%% --------------------------------------------------
%% Example: Same but filters order is hi, hi, lo, lo
%% --------------------------------------------------

design_name = sprintf('xover_lr4_%dhz_hhll_%dkhz', fc, round(fs/1000));
blob_fn = fullfile(cpath, [design_name '.bin']);
alsa_fn = fullfile(cpath, [design_name '.txt']);
tplg_fn = fullfile(tpath, [design_name '.conf']);

assign_response = [1 1 0 0];
num_responses = 2;
bm = sof_eq_iir_blob_merge(channels_in_config, ...
		       num_responses, ...
		       assign_response, ...
		       [bq_lo bq_hi]);

% Pack and write file
sof_eq_pack_export(bm, blob_fn, alsa_fn, tplg_fn, comment, howto)

%% ------------------------------------
%% Done.
%% ------------------------------------

sof_eq_paths(0);
end

%% -------------------
%% EQ design functions
%% -------------------

function eq = lo_band_iir(fs, fc)


%% Get defaults for equalizer design
eq = sof_eq_defaults();
eq.fs = fs;
eq.enable_iir = 1;
eq.iir_norm_type = 'peak';
eq.iir_norm_offs_db = 0;

% Parametric EQs are PEQ_HP1, PEQ_HP2, PEQ_LP1, PEQ_LP2, PEQ_LS1,
% PEQ_LS2, PEQ_HS1, PEQ_HS2 = 8, PEQ_PN2, PEQ_LP4, and  PEQ_HP4.
%
% Parametric EQs take as second argument the cutoff frequency in Hz
% and as second argument a dB value (can use 0 for LP2). The
% Third argument is a Q-value (can use 0 for LP2).

% Two 2nd order butterworth low-pass filters for 4th order Linkwitz–Riley
eq.peq = [ ...
                 eq.PEQ_LP2 fc 0 0 ; ...
                 eq.PEQ_LP2 fc 0 0 ; ...
         ];

%% Design EQ
eq = sof_eq_compute(eq);

%% Plot
sof_eq_plot(eq);

end

function eq = hi_band_iir(fs, fc)


%% Get defaults for equalizer design
eq = sof_eq_defaults();
eq.fs = fs;
eq.enable_iir = 1;
eq.iir_norm_type = 'peak';
eq.iir_norm_offs_db = 0;

% Two 2nd order high-pass filters for 4th order Linkwitz–Riley
eq.peq = [ ...
                 eq.PEQ_HP2 fc 0 0 ; ...
                 eq.PEQ_HP2 fc 0 0 ; ...
         ];

%% Design EQ
eq = sof_eq_compute(eq);

%% Plot
sof_eq_plot(eq);

end



% Pack and write file common function for all exports
function sof_eq_pack_export(bm, bin_fn, ascii_fn, tplg_fn, note, howto)

bp = sof_eq_iir_blob_pack(bm, 4); % IPC4

if ~isempty(bin_fn)
	sof_ucm_blob_write(bin_fn, bp);
end

if ~isempty(ascii_fn)
	sof_alsactl_write(ascii_fn, bp);
end

if ~isempty(tplg_fn)
	sof_tplg2_write(tplg_fn, bp, 'IIR', note, howto);
end

end
