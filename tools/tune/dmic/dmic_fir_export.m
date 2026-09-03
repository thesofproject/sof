% dmic_fir_export - Export FIR coefficients
%
% success=pdm_export_coef(fir, hdir)
%
% fir     - fir definition struct
% hdir    - directory for header files

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2019, Intel Corporation. All rights reserved.
%
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

function success = dmic_fir_export(fir, hdir)

if ~exist(hdir, 'dir')
	mkdir(hdir);
end

pbi = round(fir.cp*1e4);
sbi = round(fir.cs*1e4);
rpi = round(fir.rp*1e2);
rsi = round(fir.rs);

hfn = sprintf('%s/pdm_decim_int32_%02d_%04d_%04d_%03d_%03d.h', hdir, fir.m, pbi, sbi, rpi, rsi);
vfn = sprintf('fir_int32_%02d_%04d_%04d_%03d_%03d', fir.m, pbi, sbi, rpi, rsi);
sfn = sprintf('pdm_decim_int32_%02d_%04d_%04d_%03d_%03d', fir.m, pbi, sbi, rpi, rsi);

fprintf('Exporting %s ...\n', hfn);
fh = fopen(hfn, 'w');

fprintf(fh, '/* SPDX-License-Identifier: BSD-3-Clause\n');
fprintf(fh, ' *\n');
fprintf(fh, ' * Copyright(c) 2019 Intel Corporation. All rights reserved.\n');
fprintf(fh, ' */\n');
fprintf(fh, '\n');
fprintf(fh, '#include <stdint.h>\n');
fprintf(fh, '\n');

print_int_coef(fir, fh, 'int32_t', vfn);

fprintf(fh, '\n\n');
fprintf(fh, 'struct pdm_decim %s = {\n', sfn);
fprintf(fh, '\t%d, %d, %d, %d, %d, %d, %d, %s\n};\n', ...
        fir.m, fir.length, fir.shift, pbi, sbi, rpi, rsi, vfn);
fclose(fh);
success = 1;

end

function print_int_coef(fir, fh, vtype, vfn)
        fprintf(fh, 'const %s %s[%d] = {\n', ...
                vtype, vfn, fir.length);

        fprintf(fh,'\t%d', fir.coef(1));
        for n=2:fir.length
                fprintf(fh, ',\n');
                fprintf(fh,'\t%d', fir.coef(n));
        end
        fprintf(fh,'\n\n};');
end

