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

% Use functionc from EQ tool, test utils
addpath('../eq');
addpath('../../test/audio/test_utils');

%% Add needed default controls if missing

% Num of filters same as number of microphones
if isempty(bf.num_filters)
	bf.num_filters = bf.mic_n;
end

% Use all inputs linearly
if isempty(bf.input_channel_select)
	bf.input_channel_select = 0:bf.num_filters - 1;
end

% Mix all filters to all output channels
if isempty(bf.output_channel_mix)
	bf.output_channel_mix = sum(2.^(0:(bf.num_output_channels - 1))) * ...
				ones(1, bf.num_filters);
end

% All to first output stream
if isempty(bf.output_stream_mix)
	bf.output_stream_mix = zeros(1, bf.num_filters);
end


%% Build blob
filters = [];
for i=1:bf.num_filters
	bq = eq_fir_blob_quant(bf.w(:,i)', 16, 0);
	filters = [filters bq ];
end
bf.all_filters = filters;
bp = bf_blob_pack(bf);

%% Export
if isempty(bf.sofctl_fn)
	fprintf(1, 'No sof-ctl output file specified.\n');
else
	fprintf(1, 'Exporting to %s\n', bf.sofctl_fn);
	mkdir_check(bf.sofctl_path);
	eq_alsactl_write(bf.sofctl_fn, bp);
end

if isempty(bf.tplg_fn)
	fprintf(1, 'No topology output file specified.\n');
else
	fprintf(1, 'Exporting to %s\n', bf.tplg_fn);
	mkdir_check(bf.tplg_path);
	eq_tplg_write(bf.tplg_fn, bp, 'DEF_TDFB_PRIV');
end

end
