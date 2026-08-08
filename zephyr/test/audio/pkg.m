% pkg.m - Shim to silently skip 'pkg load control' when the Octave
% control package is not installed. db2mag and mag2db are provided
% as standalone .m files in this directory instead.
%
% SPDX-License-Identifier: BSD-3-Clause
% Copyright(c) 2026 Intel Corporation.

function pkg(varargin)
	% Silently ignore all pkg commands
end
