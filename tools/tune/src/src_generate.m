function src_generate(fs_in, fs_out, ctype, fs_inout, profile, qc)

% src_generate - export src conversions for given fs_in and fs_out
%
% src_generate(fs_in, fs_out <, ctype, fs_inout>>)
%
% fs_in    - vector of input sample rates (M)
% fs_out   - vector of output sample rates (N)
% ctype    - coefficient type, use 'int16','int24', 'int32', or 'float'
% fs_inout - matrix of supported conversions (MxN),
%            0 = no support, 1 = supported
%				%
% if ctype is omitted then ctype defaults to 'int16'.
%
% If fs_inout matrix is omitted this script will compute coefficients
% for all fs_in <-> fs_out combinations.
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

if (nargin == 1) || (nargin > 6)
	error('Incorrect arguments for function!');
end
if nargin == 0
	%% Default input and output rates
        fs_in = [8e3 11025 12e3 16e3 18900 22050 24e3 32e3 44100 48e3 ...
		     64e3 88.2e3 96e3 176400 192e3];

	fs_out = [8e3 11025 12e3 16e3 18900 22050 24e3 32e3 44100 48e3];

	fs_inout = [ 0 0 0 1 0 0 1 1 0 1 ; ...
		     0 0 0 0 0 0 0 0 0 1 ; ...
		     0 0 0 0 0 0 0 0 0 1 ; ...
		     1 0 0 0 0 0 1 1 0 1 ; ...
		     0 0 0 0 0 0 0 0 0 1 ; ...
		     0 0 0 0 0 0 0 0 0 1 ; ...
		     1 0 0 1 0 0 0 1 0 1 ; ...
		     1 0 0 1 0 0 1 0 0 1 ; ...
		     0 0 0 0 0 0 0 0 0 1 ; ...
		     1 1 1 1 0 1 1 1 1 0 ; ...
		     0 0 0 0 0 0 0 0 0 1 ; ...
		     0 0 0 0 0 0 0 0 0 1 ; ...
		     0 0 0 0 0 0 0 0 0 1 ; ...
		     0 0 0 0 0 0 0 0 0 1 ; ...
		     0 0 0 0 0 0 0 0 0 1 ];

	ctype = 'int32';
        profile = 'std';
	qc = 1.0;
else
	if nargin < 3
		ctype = 'int16';
	end
	if nargin < 4
		fs_inout = ones(length(fs_in), length(fs_out));
        end
        if nargin < 5
                profile = '';
        end
	if nargin < 6
		qc = 1.0;
	end
end

sio = size(fs_inout);
if (length(fs_in) ~= sio(1)) ||  (length(fs_out) ~= sio(2))
	error('Sample rates in/out matrix size mismatch!');
end

%% Exported coefficients type int16, int24, int32, float

switch ctype
       case 'int16'
	       coef_label = 'int16';
	       coef_ctype = 'int16_t';
	       coef_bits = 16;
	       coef_bytes = 2;
       case 'int24'
	       coef_label = 'int24';
	       coef_ctype = 'int32_t';
	       coef_bits = 24;
	       coef_bytes = 4;
       case 'int32'
	       coef_label = 'int32';
	       coef_ctype = 'int32_t';
	       coef_bits = 32;
	       coef_bytes = 4;
       case 'float'
	       coef_label = 'float'; coef_ctype = 'float';
	       coef_bits = 24;
	       coef_bytes = 4;
       otherwise
	       error('Request for incorrect coefficient type');
end
data_bytes = 4;

hdir = mkdir_check('include');
rdir = mkdir_check('reports');

%% Find fractional conversion factors
nfsi = length(fs_in);
nfso = length(fs_out);
l_2s = zeros(2, nfsi, nfso);
m_2s = zeros(2, nfsi, nfso);
mops_2s = zeros(2, nfsi, nfso);
pb_2s = zeros(2,nfsi, nfso);
sb_2s = zeros(2,nfsi, nfso);
taps_2s = zeros(2,nfsi, nfso);
defs.fir_delay_size = 0;
defs.out_delay_size = 0;
defs.blk_in = 0;
defs.blk_out = 0;
defs.num_in_fs = nfsi;
defs.num_out_fs = nfso;
defs.stage1_times_max = 0;
defs.stage2_times_max = 0;
defs.stage_buf_size = 0;
h = 1;
for b = 1:nfso
        for a = 1:nfsi
                fs1 = fs_in(a);
                fs2 = fs_out(b);
                [l1, m1, l2, m2] = src_factor2_lm(fs1, fs2);
                fs3 = fs1*l1/m1;
                cnv1 = src_param(fs1, fs3, coef_bits, qc);
                cnv2 = src_param(fs3, fs2, coef_bits, qc);
                if (fs2 < fs1)
                        % When decimating 1st stage passband can be limited
                        % for wider transition band
                        f_pb = fs2*cnv2.c_pb;
                        cnv1.c_pb = f_pb/min(fs1,fs3);
                end
                if (fs2 > fs1)
                        % When interpolating 2nd stage passband can be limited
                        % for wider transition band
                        f_pb = fs1*cnv1.c_pb;
                        cnv2.c_pb = f_pb/min(fs2,fs3);
                end
		if fs_inout(a,b) > 0 || (a == b)
                        if cnv2.fs1-cnv2.fs2 > eps
                                % Allow half ripple for dual stage SRC parts
                                cnv1.rp = cnv1.rp/2;
                                cnv2.rp = cnv2.rp/2;
                        end
                        src1 = src_get(cnv1);
                        src2 = src_get(cnv2);
                        k = gcd(src1.blk_out, src2.blk_in);
                        stage1_times = src2.blk_in/k;
                        stage2_times = stage1_times*src1.blk_out/src2.blk_in;
                        defs.stage1_times_max = max(defs.stage1_times_max, stage1_times);
                        defs.stage2_times_max = max(defs.stage2_times_max, stage2_times);
                        l_2s(:,a,b) = [src1.L src2.L];
                        m_2s(:,a,b) = [src1.M src2.M];
                        mops_2s(:,a,b) = [src1.MOPS src2.MOPS];
                        pb_2s(:,a,b) = [round(1e4*src1.c_pb) round(1e4*src2.c_pb)];
                        sb_2s(:,a,b) = [round(1e4*src1.c_sb) round(1e4*src2.c_sb)];
			taps_2s(:,a,b) = [src1.filter_length src2.filter_length];
                        defs.fir_delay_size = max(defs.fir_delay_size, src1.fir_delay_size);
                        defs.out_delay_size = max(defs.out_delay_size, src1.out_delay_size);
                        defs.blk_in = max(defs.blk_in, src1.blk_in);
                        defs.blk_out = max(defs.blk_out, src1.blk_out);
                        defs.fir_delay_size = max(defs.fir_delay_size, src2.fir_delay_size);
                        defs.out_delay_size = max(defs.out_delay_size, src2.out_delay_size);
                        defs.blk_in = max(defs.blk_in, src2.blk_in);
                        defs.blk_out = max(defs.blk_out, src2.blk_out);
                        defs.stage_buf_size = max(defs.stage_buf_size, src1.blk_out*stage1_times);
                        src_export_coef(src1, coef_label, coef_ctype, hdir, profile);
                        src_export_coef(src2, coef_label, coef_ctype, hdir, profile);
                end
        end
end

%% Export modes table
defs.sum_filter_lengths = src_export_table_2s(fs_in, fs_out, l_2s, m_2s, ...
        pb_2s, sb_2s, taps_2s, coef_label, coef_ctype, ...
        'sof/audio/coefficients/src/', hdir, profile);
src_export_defines(defs, coef_label, hdir, profile);

%% Print 2 stage conversion factors
fn = sprintf('%s/src_2stage.txt', rdir);
fh = fopen(fn,'w');
fprintf(fh,'\n');
fprintf(fh,'Dual stage fractional SRC: Ratios\n');
fprintf(fh,'%8s, ', 'in \ out');
for b = 1:nfso
        fprintf(fh,'%12.1f, ', fs_out(b)/1e3);
end
fprintf(fh,'\n');
for a = 1:nfsi
        fprintf(fh,'%8.1f, ', fs_in(a)/1e3);
        for b = 1:nfso
                cstr = print_ratios(l_2s, m_2s, a, b);
                fprintf(fh,'%12s, ', cstr);
        end
        fprintf(fh,'\n');
end
fprintf(fh,'\n');

%% Print 2 stage MOPS
fprintf(fh,'Dual stage fractional SRC: MOPS\n');
fprintf(fh,'%8s, ', 'in \ out');
for b = 1:nfso
        fprintf(fh,'%8.1f, ', fs_out(b)/1e3);
end
fprintf(fh,'\n');
for a = 1:nfsi
        fprintf(fh,'%8.1f, ', fs_in(a)/1e3);
        for b = 1:nfso
                mops = sum(mops_2s(:,a,b));
                if sum(l_2s(:,a,b)) < eps
                        mops_str = 'x';
                else
                        mops_str = sprintf('%.2f', mops);
                end
                fprintf(fh,'%8s, ', mops_str);
        end
        fprintf(fh,'\n');
end
fprintf(fh,'\n');

%% Print 2 stage MOPS per stage
fprintf(fh,'Dual stage fractional SRC: MOPS per stage\n');
fprintf(fh,'%10s, ', 'in \ out');
for b = 1:nfso
        fprintf(fh,'%10.1f, ', fs_out(b)/1e3);
end
fprintf(fh,'\n');
for a = 1:nfsi
        fprintf(fh,'%10.1f, ', fs_in(a)/1e3);
        for b = 1:nfso
                mops = mops_2s(:,a,b);
                if sum(l_2s(:,a,b)) < eps
                        mops_str = 'x';
                else
                        mops_str = sprintf('%.2f+%.2f', mops(1), mops(2));
                end
                fprintf(fh,'%10s, ', mops_str);
        end
        fprintf(fh,'\n');
end
fprintf(fh,'\n');

fprintf(fh,'Coefficient RAM %.1f kB\n', ...
        defs.sum_filter_lengths*coef_bytes/1024);
fprintf(fh,'Max. data RAM %.1f kB\n', ...
	(defs.fir_delay_size + defs.out_delay_size+defs.stage_buf_size) ...
        * data_bytes/1024);

fprintf(fh,'\n');
fclose(fh);
type(fn);

end

function d = mkdir_check(d)
if exist(d) ~= 7
        mkdir(d);
end
end

function cstr = print_ratios(l_2s, m_2s, a, b)
l1 = l_2s(1,a,b);
m1 = m_2s(1,a,b);
l2 = l_2s(2,a,b);
m2 = m_2s(2,a,b);
if l1+m2+l2+m2 == 0
        cstr = 'x';
else
        if m2 == 1
                if l2 == 1
                        cstr2 = '';
                else
                        cstr2 = sprintf('*%d', l2);
                end
        else
                cstr2 = sprintf('*%d/%d', l2, m2);
        end
        if m1 == 1
                cstr1 = sprintf('%d', l1);
        else
                cstr1 = sprintf('%d/%d', l1, m1);
        end
        cstr = sprintf('%s%s', cstr1, cstr2);
end
end
