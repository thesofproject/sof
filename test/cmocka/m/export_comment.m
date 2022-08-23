% export_comment(fh, text)
%
% Inputs
%   fh - file handle
%   text - text comment

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright(c) 2022 Intel Corporation. All rights reserved.

function export_comment(fh, text)

	fprintf(fh, '/* %s */\n\n', text);

end
