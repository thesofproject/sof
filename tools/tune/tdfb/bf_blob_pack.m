function blob8 = bf_blob_pack(bf)

%% Pack TDFB struct to bytes
%
% blob8 = bf_blob_pack(bf)
%
% bf ..... TDFB design data struct input
% blob8 .. Packed bytes blob output
%

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright(c) 2020 Intel Corporation. All rights reserved.
%
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

%% Check for sane parameters
if bf.num_filters < 1 || bf.num_filters > 16
	error('Invalid number of filters');
end

if bf.num_output_channels < 1 || bf.num_output_channels > 8
	error('Invalid number of output channels');
end

if bf.num_output_streams < 1 || bf.num_output_streams > 8
	error('Invalid number of output streams');
end

if length(bf.input_channel_select) ~= bf.num_filters
	error('input_channel_select length does not match');
end

if length(bf.output_channel_mix) ~= bf.num_filters
	error('output_channel_mix length does not match');
end

if length(bf.output_stream_mix) ~= bf.num_filters
	error('output_stream_mix length does not match');
end

%% Endianness of blob
switch lower(bf.endian)
        case 'little'
                sh16 = [0 -8];
                sh32 = [0 -8 -16 -24];
        case 'big'
                sh16 = [-8 0];
                sh32 = [-24 -16 -8 0];
        otherwise
                error('Unknown endianness');
end

%% Header format is
%	uint32_t size;
%	uint16_t num_filters;
%	uint16_t num_output_channels;
%	uint16_t num_output_streams;
%	uint16_t reserved16;
%	uint32_t reserved32[4];
%	int16_t data[];
%
% data[] is
% int16_t fir_filter1[length_filter1];  Multiple of 4 taps and 32 bit align
% int16_t fir_filter2[length_filter2];  Multiple of 4 taps and 32 bit align
%		...
% int16_t fir_filterN[length_filterN];  Multiple of 4 taps and 32 bit align
% int16_t input_channel_select[num_filters];  0 = ch0, 1 = 1ch1, ..
% int16_t output_channel_mix[num_filters];
% int16_t output_stream_mix[num_filters];

%% Pack as 16 bits
nh16 = 14;
h16 = zeros(1, nh16, 'int16');
nc16 = length(bf.all_filters);
na16 = 4 * bf.num_angles;
nl16 = 4 * bf.mic_n;
if bf.beam_off_defined
	nm16 = 4 * bf.num_filters;
else
	nm16 = 3 * bf.num_filters;
end

nb16 = ceil((nh16 + nc16 + nm16 + na16 + nl16)/2)*2;
h16(1) = 2 * nb16;
h16(2) = 0;
h16(3) = bf.num_filters;
h16(4) = bf.num_output_channels;
h16(5) = bf.num_output_streams;
h16(6) = 0;
h16(7) = bf.mic_n;
h16(8) = bf.num_angles;
h16(9) = bf.beam_off_defined;
h16(10) = bf.track_doa;
h16(11) = bf.angle_enum_mult;
h16(12) = bf.angle_enum_offs;

%% Merge header and coefficients, make even number of int16 to make it
%  multiple of int32
blob16 = zeros(1,nb16, 'int16');
blob16(1:nh16) = h16;
i1 = nh16 + 1;
i2 = i1 + nc16 -1;
blob16(i1:i2) = int16(bf.all_filters);
i1 = i2 + 1;
i2 = i1 + bf.num_filters - 1;
blob16(i1:i2) = int16(bf.input_channel_select);
i1 = i2 + 1;
i2 = i1 + bf.num_filters - 1;
blob16(i1:i2) = int16(bf.output_channel_mix);
i1 = i2 + 1;
i2 = i1 + bf.num_filters - 1;
blob16(i1:i2) = int16(bf.output_stream_mix);

if (bf.beam_off_defined)
	i1 = i2 + 1;
	i2 = i1 + bf.num_filters - 1;
	blob16(i1:i2) = int16(bf.output_channel_mix_beam_off);
end

for i = 1:bf.num_angles
	i1 = i2 + 1;
	i2 = i1 + 4 - 1;
	blob16(i1:i2) = int16([ bf.steer_az(i) bf.steer_el(i) (i - 1)*bf.num_filters 0 ]);
end

% Coordinates are Q4.12 m
scale=2^12;
for i = 1:bf.mic_n
	i1 = i2 + 1;
	i2 = i1 + 4 - 1;
	loc = [ round(bf.mic_x(i)*scale) round(bf.mic_y(i)*scale) round(bf.mic_z(i)*scale) 0 ];
	blob16(i1:i2) = int16(loc);
end

%% Pack as 8 bits
nbytes_data = nb16 * 2;

%% Get ABI information
[abi_bytes, nbytes_abi] = eq_get_abi(nbytes_data);

%% Initialize uint8 array with correct size
nbytes = nbytes_abi + nbytes_data;
blob8 = zeros(1, nbytes, 'uint8');

%% Inset ABI header
blob8(1:nbytes_abi) = abi_bytes;
j = nbytes_abi + 1;

%% Component data
for i = 1:length(blob16)
        blob8(j:j+1) = w16b(blob16(i), sh16);
        j = j+2;
end

%% Done
fprintf('Blob size is %d bytes.\n', nbytes);

end

function bytes = w16b(word, sh)
bytes = uint8(zeros(1,2));
bytes(1) = bitand(bitshift(word, sh(1)), 255);
bytes(2) = bitand(bitshift(word, sh(2)), 255);
end

function bytes = w32b(word, sh)
bytes = uint8(zeros(1,4));
bytes(1) = bitand(bitshift(word, sh(1)), 255);
bytes(2) = bitand(bitshift(word, sh(2)), 255);
bytes(3) = bitand(bitshift(word, sh(3)), 255);
bytes(4) = bitand(bitshift(word, sh(4)), 255);
end
