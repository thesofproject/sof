%%
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

function test = test_run(test)

if isfield(test, 'ex') == 0
	test.ex = './comp_run.sh';
end

if exist(fullfile('./', test.ex),'file') == 2
	ex = test.ex;
else
        error('Can''t find executable %s', test.ex);
end

fn_config = sprintf("%s_config.sh", tempname);
fh = fopen(fn_config, 'w');

if isfield(test, 'fs_in') == 0
	test.fs_in = test.fs;
end

if isfield(test, 'bits_in') == 0
	test.bits_in = test.bits;
end

if isfield(test, 'nch_in') == 0
	test.nch_in = test.nch;
end

fprintf(fh, '#!/bin/sh\n', test.comp);
fprintf(fh, 'COMP=\"%s\"\n', test.comp);
fprintf(fh, 'DIRECTION=playback\n');
fprintf(fh, 'BITS_IN=%d\n', test.bits_in);
if isfield(test, 'bits_out') && (test.bits_in ~= test.bits_out)
	fprintf(fh, 'BITS_IN=%d\n', test.bits_out);
end

fprintf(fh, 'CHANNELS_IN=%d\n', test.nch_in);
if isfield(test, 'nch_out') && (test.nch_in ~= test.nch_out)
	fprintf(fh, 'CHANNELS_OUT=%d\n', test.nch_out);
end

fprintf(fh, 'FS_IN=%d\n', test.fs_in);
if isfield(test, 'fs_out') && (test.fs_in ~= test.fs_out)
	fprintf(fh, 'FS_OUT=%d\n', test.fs_out);
end

fprintf(fh, 'FN_IN=\"%s\"\n', test.fn_in);
fprintf(fh, 'FN_OUT=\"%s\"\n', test.fn_out);

if isfield(test, 'extra_opts')
	fprintf(fh, 'EXTRA_OPTS=\"%s\"\n', test.extra_opts);
end

if isfield(test, 'trace')
	fprintf(fh, 'FN_TRACE=\"%s\"\n', test.trace);
end

% Override defaults in comp_run.sh
fprintf(fh, 'VALGRIND=no\n', test.fs_in);
fclose(fh);

arg = sprintf('-t %s', fn_config);
rcmd = sprintf('%s %s', ex, arg);
fprintf('Running ''%s''...\n', rcmd);
system(rcmd);
delete(fn_config);

end
