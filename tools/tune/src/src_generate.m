function src_generate(fs_in, fs_out, fs_inout, cfg);

% src_generate - export src conversions for given fs_in and fs_out
%
% src_generate(fs_in, fs_out <, fs_inout, <cfg>>)
%
% fs_in     - vector of input sample rates (M)
% fs_out    - vector of output sample rates (N)
% fs_inout  - matrix of supported conversions (MxN),
%             0 = no support, 1 = supported
% cfg       - configuration struct with fields
%   ctype   - coefficient type, use 'int16','int24', 'int32', or 'float'
%   profile - differentiate set with identifier, e.g. 'std'
%   quality - quality factor, usually 1.0
%   speed   - optimize speed, gives higher RAM size, usually 0
%
% If fs_inout matrix is omitted this script will compute coefficients
% for all fs_in <-> fs_out combinations.
%
% If cfg is omitted the script will assume 'int32', 'std', 1.0, 0.
%

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2016-2020, Intel Corporation. All rights reserved.
%
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

if (nargin < 2) || (nargin > 4)
	error('Incorrect arguments for function!');
end
if nargin < 4
	cfg.ctype = 'int32';
	cfg.profile = 'std';
	cfg.quality = 1.0;
	cfg.speed = 0;
end
if nargin < 3
	fs_inout = ones(length(fs_in), length(fs_out));
end

sio = size(fs_inout);
if (length(fs_in) ~= sio(1)) ||  (length(fs_out) ~= sio(2))
	error('Sample rates in/out matrix size mismatch!');
end

%% Exported coefficients type int16, int24, int32, float

switch cfg.ctype
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
	       coef_label = 'float';
	       coef_ctype = 'float';
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
		% If the interpolate/decimate factors are low, use single step
		% conversion. The increases RAM consumption but lowers the MCPS.
		if cfg.speed && max(l1 * l2, m1 * m2) < 30
			l1 = l1 * l2;
			l2 = 1;
			m1 = m1 * m2;
			m2 = 1;
		end
                fs3 = fs1*l1/m1;
                cnv1 = src_param(fs1, fs3, coef_bits, cfg.quality);
                cnv2 = src_param(fs3, fs2, coef_bits, cfg.quality);
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
		if fs_inout(a,b) > 0 || abs(fs1 - fs2) < eps
                        if cnv2.fs1-cnv2.fs2 > eps
                                % Allow half ripple for dual stage SRC parts
                                cnv1.rp = cnv1.rp/2;
                                cnv2.rp = cnv2.rp/2;
				% Distribute gain also
				cnv1.gain = cnv1.gain/2;
				cnv2.gain = cnv2.gain/2;
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
                        src_export_coef(src1, coef_label, coef_ctype, hdir, cfg.profile);
                        src_export_coef(src2, coef_label, coef_ctype, hdir, cfg.profile);
                end
        end
end

%% Export modes table
defs.sum_filter_lengths = src_export_table_2s(fs_in, fs_out, l_2s, m_2s, ...
        pb_2s, sb_2s, taps_2s, coef_label, coef_ctype, ...
        'sof/audio/coefficients/src/', hdir, cfg.profile);
src_export_defines(defs, coef_label, hdir, cfg.profile);

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
if ~exist(fullfile('.', d),'dir')
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
