% export_ndefine(fh, dn, val)
%
% Inputs
%   fh - file handle
%   dn - define macro name
%   val - value for macro

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright(c) 2022 Intel Corporation. All rights reserved.

function export_ndefine(fh, dn, dval)

	fprintf(fh, '#define %s  %d\n', dn, dval);

end
