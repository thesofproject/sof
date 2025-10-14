% dmic_fir_export - Export FIR coefficients
%
% vfn_arr = pdm_export_coef(fir, hdir, prev_vfn_arr)
%
% fir     - fir definition struct
% hdir    - directory for header files

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2023, Intel Corporation. All rights reserved.
%
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

function vfn_arr = dmic_fir_export_alsa(fir, hdir, prev_vfn_arr)

if ~exist(hdir, 'dir')
	mkdir(hdir);
end

pbi = round(fir.cp * 1e4);
sbi = round(fir.cs * 1e4);
rpi = round(fir.rp * 1e2);
rsi = round(fir.rs);

hfn = sprintf('%s/pdm-decim-fir.h', hdir);
vfn = sprintf('fir_int32_%02d_%04d_%04d_%03d_%03d', fir.m, pbi, sbi, rpi, rsi);
sfn = sprintf('pdm_decim_int32_%02d_%04d_%04d_%03d_%03d', fir.m, pbi, sbi, rpi, rsi);

fprintf('Exporting %s ...\n', hfn);

if exist(hfn, 'file') == 2
	% Avoid to export duplicate
	if max(ismember(prev_vfn_arr, vfn)) > 0
		vfn_arr = prev_vfn_arr;
		return
	else
		fh = fopen(hfn, 'a');
		vfn_arr = [prev_vfn_arr; vfn];
	end
else
	fh = fopen(hfn, 'w');
	fprintf(fh, '// SPDX-License-Identifier: BSD-3-Clause\n');
	fprintf(fh, '//\n');
	fprintf(fh, '// Copyright(c) 2021-2023 Intel Corporation. All rights reserved.\n');
	fprintf(fh, '//\n');
	fprintf(fh, '// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>\n');
	fprintf(fh, '//         Jaska Uimonen <jaska.uimonen@linux.intel.com>\n\n');
	fprintf(fh, '#ifndef __SOF_AUDIO_COEFFICIENTS_PDM_DECIM_PDM_DECIM_FIR_H__\n');
	fprintf(fh, '#define __SOF_AUDIO_COEFFICIENTS_PDM_DECIM_PDM_DECIM_FIR_H__\n\n');
	fprintf(fh, '#include <stdint.h>\n\n');
	fprintf(fh, 'struct pdm_decim {\n');
	fprintf(fh, '	int decim_factor;\n');
	fprintf(fh, '	int length;\n');
	fprintf(fh, '	int shift;\n');
	fprintf(fh, '	int relative_passband;\n');
	fprintf(fh, '	int relative_stopband;\n');
	fprintf(fh, '	int passband_ripple;\n');
	fprintf(fh, '	int stopband_ripple;\n');
	fprintf(fh, '	const int32_t *coef;\n');
	fprintf(fh, '};\n');
	vfn_arr = { vfn };
end

print_int_coef(fir, fh, 'int32_t', vfn);

fprintf(fh, '\n\n');
fprintf(fh, 'struct pdm_decim %s = {\n', sfn);
fprintf(fh, '\t%d, %d, %d, %d, %d, %d, %d, %s\n};\n', ...
        fir.m, fir.length, fir.shift, pbi, sbi, rpi, rsi, vfn);
fclose(fh);

end

function print_int_coef(fir, fh, vtype, vfn)

coefs_per_line = 5;
n = 1;

fprintf(fh, '\n');
fprintf(fh, 'const %s %s[%d] = {\n', ...
	vtype, vfn, fir.length);

while n <= fir.length
	fprintf(fh, '\t');
	nremain = fir.length - n + 1;
	ncoef = min(coefs_per_line, nremain);
	for i = 1 : ncoef
		fprintf(fh,'%d', fir.coef(n));
		if n < fir.length
			fprintf(fh, ',');
			if i < ncoef
				fprintf(fh, ' ');
			end
		end
		n = n + 1;
	end
	fprintf(fh, '\n');
end
fprintf(fh,'};');

end

