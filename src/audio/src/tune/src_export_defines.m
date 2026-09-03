function src_export_defines(defs, ctype, hdir, profile)

% src_export_defines - Exports the constants to header files
%
% src_export_defines(defs, ctype, hdir)
%
% defs    - defs struct
% ctype   - e.g. 'int24' appended to file name
% hdir    - directory for header file
% profile - string to append to file name
%

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2016-2020, Intel Corporation. All rights reserved.
%
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

if nargin < 4
        profile = '';
end

if isempty(profile)
        hfn = sprintf('src_%s_define.h', ctype);
else
        hfn = sprintf('src_%s_%s_define.h', profile, ctype);
end

fh = fopen(fullfile(hdir,hfn), 'w');
pu = upper(profile);
cu = upper(ctype);
y = datestr(now(), 'yyyy');
def = sprintf('__SOF_AUDIO_COEFFICIENTS_SRC_SRC_%s_%s_DEFINE_H__', pu, cu);

fprintf(fh, '/* SPDX-License-Identifier: BSD-3-Clause\n');
fprintf(fh, ' *\n');
fprintf(fh, ' * Copyright(c) %s Intel Corporation. All rights reserved.\n', y);
fprintf(fh, ' *\n');
fprintf(fh, ' */\n');
fprintf(fh, '\n');
fprintf(fh, '#ifndef %s\n', def);
fprintf(fh, '#define %s\n', def);
fprintf(fh, '\n');
fprintf(fh, '/* SRC constants */\n');
fprintf(fh, '#define MAX_FIR_DELAY_SIZE %d\n', defs.fir_delay_size);
fprintf(fh, '#define MAX_OUT_DELAY_SIZE %d\n', defs.out_delay_size);
fprintf(fh, '#define MAX_BLK_IN %d\n', defs.blk_in);
fprintf(fh, '#define MAX_BLK_OUT %d\n', defs.blk_out);
fprintf(fh, '#define NUM_IN_FS %d\n', defs.num_in_fs);
fprintf(fh, '#define NUM_OUT_FS %d\n', defs.num_out_fs);
fprintf(fh, '#define STAGE1_TIMES_MAX %d\n', defs.stage1_times_max);
fprintf(fh, '#define STAGE2_TIMES_MAX %d\n', defs.stage2_times_max);
fprintf(fh, '#define STAGE_BUF_SIZE %d\n', defs.stage_buf_size);
fprintf(fh, '#define NUM_ALL_COEFFICIENTS %d\n', defs.sum_filter_lengths);
fprintf(fh, '\n');
fprintf(fh, '#endif /* %s */\n', def);
fclose(fh);

end
