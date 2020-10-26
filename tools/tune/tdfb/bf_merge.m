% bfm = bf_merge(bf1, bf2, bf3, bf4)

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2020, Intel Corporation. All rights reserved.
%
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

function bfm = bf_merge(bf1, bf2, bf3, bf4)

if nargin > 2
	error('Current implementation can merge only two beams configuration');
end

% Check that filter lengths match
s1 = size(bf1.w);
n1 = s1(2);
s2 = size(bf2.w);
n2 = s2(2);
if s1(1) ~= s2(1)
	error();
end

% Get data from bf1, then update fields impacted by merge
bfm = bf1;

% Merge coefficients
bfm.w = zeros(s1(1), n1 + n2);
for i = 1:n1
	bfm.w(:,i) = bf1.w(:,i);
end
for i = 1:n2
	bfm.w(:,n1 + i) = bf2.w(:,i);
end

% Merge filter inputs specification
bfm.input_channel_select = [bf1.input_channel_select bf2.input_channel_select];

% Merge filter outputs specification
bfm.output_channel_mix = [bf1.output_channel_mix bf2.output_channel_mix];
if isempty(bf1.output_stream_mix)
	bf1.output_stream_mix = zeros(1, bf1.num_filters);
end
if isempty(bf2.output_stream_mix)
	bf2.output_stream_mix = zeros(1, bf2.num_filters);
end
bfm.output_stream_mix = [bf1.output_stream_mix bf2.output_stream_mix];

bfm.num_filters = bf1.num_filters + bf2.num_filters;
bfm.num_output_channels = floor(log(max(union(bf1.output_channel_mix, bf2.output_channel_mix)))/log(2)) + 1;
bfm.num_output_streams = max(union(bf1.output_stream_mix, bf2.output_stream_mix)) + 1;

fprintf(1, 'Merge is ready\n');
fprintf(1, 'Number of filters %d\n', bfm.num_filters);
fprintf(1, 'Number of output channels %d\n', bfm.num_output_channels);
fprintf(1, 'Number of output streams %d\n', bfm.num_output_streams);

end
