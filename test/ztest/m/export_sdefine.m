% export_sdefine(fh, dn, str)
%
% Inputs
%   fh - file handle
%   dn - define macro name
%   str - string for macro

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright(c) 2022 Intel Corporation. All rights reserved.

function export_sdefine(fh, dn, dstr)

	fprintf(fh, '#define %s  %s\n', dn, dstr);

end
