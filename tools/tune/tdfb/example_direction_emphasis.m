% Design 48 kHz and 16 kHz emphasis filter for crosscorrelation
% operation in sound direction estimation.

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2016-2020, Intel Corporation. All rights reserved.
%
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

function example_direction_emphasis()

addpath('../eq');

%% --------------------------------------------------
%% Example: TDFB emphasis filter 16 kHz and 48 kHz
%% --------------------------------------------------
tdfb_emphasis(48e3, 'tdfb_iir_emphasis_48k.h', 'iir_emphasis_48k');
tdfb_emphasis(16e3, 'tdfb_iir_emphasis_16k.h', 'iir_emphasis_16k');

%% ------------------------------------
%% Done.
%% ------------------------------------

end

%% -------------------
%% EQ design functions
%% -------------------

function eq = tdfb_emphasis(fs, fn, vn)

%% Get defaults for equalizer design
eq = eq_defaults();
eq.fs = fs;
eq.enable_iir = 1;
eq.norm_type = 'peak';
eq.norm_offs_db = 0;

%% Parametric EQ
eq.peq = [ ...
                 eq.PEQ_HP2  800 NaN NaN ; ...
                 eq.PEQ_LP2 4000 NaN NaN ; ...
         ];

%% Design EQ
eq = eq_compute(eq);

%% Plot
eq_plot(eq);

%% Quantize and pack filter coefficients plus shifts etc.
bq = eq_iir_blob_quant(eq.p_z, eq.p_p, eq.p_k);

%% Build blob
channels_in_config = 2;  % Setup max 4 channels EQ
assign_response = [0 0]; % Order: lo, hi, lo, hi
num_responses = 1;       % Two responses: lo, hi
bm = eq_iir_blob_merge(channels_in_config, ...
		       num_responses, ...
		       assign_response, ...
		       bq);

%% Pack and write file
bp = eq_iir_blob_pack(bm);
export_c_eq_uint32t(fn, bp, vn, 1);

end
