% bf_export(bf)
%
% Inputs
% bf.sofctl_fn ..... filename of ascii text format blob
% bf.tplg_fn ....... filename of topology m4 format blob
% bf ............... the design procedure output

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2020, Intel Corporation. All rights reserved.
%
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

function bf_export(bf)

% Use functionc from EQ tool
addpath('../eq');

%% Build blob
filters = [];
for i=1:bf.num_filters
	bq = eq_fir_blob_quant(bf.w(:,i)', 16, 0);
	filters = [filters bq ];
end
bf.all_filters = filters;
bp = bf_blob_pack(bf);

%% Export
eq_alsactl_write(bf.sofctl_fn, bp);
eq_tplg_write(bf.tplg_fn, bp, 'DEF_TDFB_PRIV');

end
