function [x, nx] = load_test_input(test)

%% [x, n] = load_test_input(t)
%
% Input
% t.fn_in      - file name to load
% t.bits_in    - word length of data
% t.fmt        - file format 'raw' or 'txt
% t.ch         - channel to extract
% t.nch        - number of channels in (interleaved) data
%
% Output
% x - samples
% n - number of samples
%

% SPDX-License-Identifier: BSD-3-Clause
% Copyright(c) 2020 Intel Corporation. All rights reserved.
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

tmp = test;
tmp.fn_out = test.fn_in;
tmp.bits_out = test.bits_in;
[x, nx] = load_test_output(tmp);

end
