%% Design effect EQs and bundle them to parameter block

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2016-2020, Intel Corporation. All rights reserved.
%
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

function sof_example_iir_bandsplit()

%% Common definitions
fs = 48e3;
sof_tools = '../../../../tools';
tpath = fullfile(sof_tools, 'topology/topology1/m4');
cpath = fullfile(sof_tools, 'ctl/ipc3/eq_iir');
priv = 'DEF_EQIIR_PRIV';

sof_eq_paths(1);

%% --------------------------------------------------
%% Example: Band-split 2ch to 4ch low and high bands
%% --------------------------------------------------
blob_fn = fullfile(cpath, 'bandsplit.blob');
alsa_fn = fullfile(cpath, 'bandsplit.txt');
tplg_fn = fullfile(tpath, 'eq_iir_bandsplit.m4');
comment = 'Bandsplit, created with example_iir_bandsplit.m';

%% Design IIR loudness equalizer
eq_lo = lo_band_iir(fs);
eq_hi = hi_band_iir(fs);

%% Quantize and pack filter coefficients plus shifts etc.
bq_lo = sof_eq_iir_blob_quant(eq_lo.p_z, eq_lo.p_p, eq_lo.p_k);
bq_hi = sof_eq_iir_blob_quant(eq_hi.p_z, eq_hi.p_p, eq_hi.p_k);

%% Build blob
channels_in_config = 4;      % Setup max 4 channels EQ
assign_response = [0 0 1 1]; % Order: lo, lo, hi, hi
num_responses = 2;           % Two responses: lo, hi
bm = sof_eq_iir_blob_merge(channels_in_config, ...
		       num_responses, ...
		       assign_response, ...
		       [bq_lo bq_hi]);

%% Pack and write file
sof_eq_pack_export(bm, blob_fn, alsa_fn, tplg_fn, priv, comment)

%% ------------------------------------
%% Done.
%% ------------------------------------

sof_eq_paths(0);
end

%% -------------------
%% EQ design functions
%% -------------------

function eq = lo_band_iir(fs)


%% Get defaults for equalizer design
eq = sof_eq_defaults();
eq.fs = fs;
eq.enable_iir = 1;
eq.iir_norm_type = 'peak';
eq.iir_norm_offs_db = 0;

%% Manually setup low-shelf and high shelf parametric equalizers
%
% Parametric EQs are PEQ_HP1, PEQ_HP2, PEQ_LP1, PEQ_LP2, PEQ_LS1,
% PEQ_LS2, PEQ_HS1, PEQ_HS2 = 8, PEQ_PN2, PEQ_LP4, and  PEQ_HP4.
%
% Parametric EQs take as second argument the cutoff frequency in Hz
% and as second argument a dB value (or NaN when not applicable) . The
% Third argument is a Q-value (or NaN when not applicable).

% Low-pass at 2 kHz, add a high-pass at 80 Hz for a small woofer
eq.peq = [ ...
                 eq.PEQ_HP2 80 NaN NaN ; ...
                 eq.PEQ_LP2 2000 NaN NaN ; ...
         ];

%% Design EQ
eq = sof_eq_compute(eq);

%% Plot
sof_eq_plot(eq);

end

function eq = hi_band_iir(fs)


%% Get defaults for equalizer design
eq = sof_eq_defaults();
eq.fs = fs;
eq.enable_iir = 1;
eq.iir_norm_type = 'peak';
eq.iir_norm_offs_db = 0;

%% Manually setup low-shelf and high shelf parametric equalizers
%
% Parametric EQs are PEQ_HP1, PEQ_HP2, PEQ_LP1, PEQ_LP2, PEQ_LS1,
% PEQ_LS2, PEQ_HS1, PEQ_HS2 = 8, PEQ_PN2, PEQ_LP4, and  PEQ_HP4.
%
% Parametric EQs take as second argument the cutoff frequency in Hz
% and as second argument a dB value (or NaN when not applicable) . The
% Third argument is a Q-value (or NaN when not applicable).

% High-pass at 2 kHz for a tweeter
eq.peq = [ ...
                 eq.PEQ_HP2 2000 NaN NaN ; ...
         ];

%% Design EQ
eq = sof_eq_compute(eq);

%% Plot
sof_eq_plot(eq);

end



% Pack and write file common function for all exports
function sof_eq_pack_export(bm, bin_fn, ascii_fn, tplg_fn, priv, note)

bp = sof_eq_iir_blob_pack(bm);

if ~isempty(bin_fn)
	sof_ucm_blob_write(bin_fn, bp);
end

if ~isempty(ascii_fn)
	sof_eq_alsactl_write(ascii_fn, bp);
end

if ~isempty(tplg_fn)
	sof_eq_tplg_write(tplg_fn, bp, priv, note);
end

end
