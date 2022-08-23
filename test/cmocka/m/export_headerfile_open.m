% fh = export_headerfile_open(headerfn, corp)
%
% Inputs
%   headerfn - File name of exported header file
%   corp - optional corporation name, defaults to Intel Corporation
%
% Outputs
%   fh - file handle

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright(c) 2022 Intel Corporation. All rights reserved.

function fh = export_headerfile_open(headerfn, corp)

	if nargin < 2
		corp = 'Intel Corporation';
	end

	fh = fopen(headerfn, 'w');
	if fh < 0
		error('Could not open file');
	end
	fprintf(fh, '/* SPDX-License-Identifier: BSD-3-Clause\n');
	fprintf(fh, ' *\n');
	fprintf(fh, ' * Copyright(c) %s %s. All rights reserved.\n', ...
		datestr(now, 'yyyy'), corp);
	fprintf(fh, ' */\n\n');
end
