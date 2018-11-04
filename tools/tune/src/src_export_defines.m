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


% Copyright (c) 2016, Intel Corporation
% All rights reserved.
%
% Redistribution and use in source and binary forms, with or without
% modification, are permitted provided that the following conditions are met:
%   * Redistributions of source code must retain the above copyright
%     notice, this list of conditions and the following disclaimer.
%   * Redistributions in binary form must reproduce the above copyright
%     notice, this list of conditions and the following disclaimer in the
%     documentation and/or other materials provided with the distribution.
%   * Neither the name of the Intel Corporation nor the
%     names of its contributors may be used to endorse or promote products
%     derived from this software without specific prior written permission.
%
% THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
% AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
% IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
% ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
% LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
% CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
% SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
% INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
% CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
% ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
% POSSIBILITY OF SUCH DAMAGE.
%
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
%

if nargin < 4
        profile = '';
end

if isempty(profile)
        hfn = sprintf('src_%s_define.h', ctype);
else
        hfn = sprintf('src_%s_%s_define.h', profile, ctype);
end
fh = fopen(fullfile(hdir,hfn), 'w');
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
fclose(fh);

end
