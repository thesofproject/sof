% sof_bf_export(bf)
%
% Inputs
% bf.sofctl3_fn .... filename of ascii text format blob
% bf.sofctl4_fn .... filename of ascii text format blob
% bf.ucmbin3_fn .... filename of binary format blob for UCM (IPC3)
% bf.ucmbin4_fn .... filename of binary format blob for UCM (IPC4)
% bf.tplg1_fn ...... filename of topology m4 format blob
% bf.tplg2_fn ...... filename of topology m4 format blob
% bf ............... the design procedure output

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2020, Intel Corporation. All rights reserved.
%
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

function bf = sof_bf_export(bf)

% Use functions from common, test utils
sof_bf_paths(true);

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

% Mix all filters to all output channels
if isempty(bf.output_channel_mix_beam_off)
	if (bf.num_output_channels == bf.num_filters)
		bf.output_channel_mix_beam_off = 2.^(0:(bf.num_output_channels - 1));
	else
		fprintf(1, 'Number of output channels: %d\n', bf.num_output_channels);
		fprintf(1, 'Number of filters: %d\n', bf.num_filters);
		error('Need to specify output_channel_mix_beam_off');
	end
end

% All to first output stream
if isempty(bf.output_stream_mix)
	bf.output_stream_mix = zeros(1, bf.num_filters);
end


%% Quantize filters
filters = [];
for j=1:bf.num_angles
	for i=1:bf.num_filters
		coefs = squeeze(bf.w(:,i,j));
		bq = sof_eq_fir_blob_quant(coefs, 16, 0);
		filters = [filters bq ];
	end
end

%% Add beam-off preset
if bf.beam_off_defined
	b_pass = [1];
	bq = sof_eq_fir_blob_quant(b_pass, 16, 0);
	for i=1:bf.num_filters
		filters = [filters bq ];
	end
end

%% Build blob
bf.all_filters = filters;
bp3 = sof_bf_blob_pack(bf, 3);
bp4 = sof_bf_blob_pack(bf, 4);

%% Export
if isempty(bf.sofctl3_fn)
	fprintf(1, 'No sof-ctl3 output file specified.\n');
else
	fprintf(1, 'Exporting to %s\n', bf.sofctl3_fn);
	mkdir_check(bf.sofctl3_path);
	alsactl_write(bf.sofctl3_fn, bp3);
end

if isempty(bf.sofctl4_fn)
	fprintf(1, 'No sof-ctl4 output file specified.\n');
else
	fprintf(1, 'Exporting to %s\n', bf.sofctl4_fn);
	mkdir_check(bf.sofctl4_path);
	alsactl_write(bf.sofctl4_fn, bp4);
end

if isempty(bf.export_note)
	export_note = sprintf("Exported with script example_%s_array.m", bf.array);
else
	export_note = bf.export_note;
end

if isempty(bf.tplg1_fn)
	fprintf(1, 'No topology1 output file specified.\n');
else
	fprintf(1, 'Exporting to %s\n', bf.tplg1_fn);
	mkdir_check(bf.tplg1_path);
	tplg_write(bf.tplg1_fn, bp3, 'DEF_TDFB_PRIV', export_note, bf.export_howto);
end

if isempty(bf.tplg2_fn)
	fprintf(1, 'No topology2 output file specified.\n');
else
	fprintf(1, 'Exporting to %s\n', bf.tplg2_fn);
	mkdir_check(bf.tplg2_path);
	tplg2_write(bf.tplg2_fn, bp4, "tdfb_config", export_note, bf.export_howto);
end

if ~isempty(bf.ucmbin3_fn)
	fprintf(1, 'Exporting to %s\n', bf.ucmbin3_fn);
	mkdir_check(bf.sofctl3_path);
	sof_ucm_blob_write(bf.ucmbin3_fn, bp3);
end

if ~isempty(bf.ucmbin4_fn)
	fprintf(1, 'Exporting to %s\n', bf.ucmbin4_fn);
	mkdir_check(bf.sofctl4_path);
	sof_ucm_blob_write(bf.ucmbin4_fn, bp4);
end

sof_bf_paths(false);

end
