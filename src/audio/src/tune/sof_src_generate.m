function sof_src_generate(fs_in, fs_out, fs_inout, cfg);

% sof_src_generate - export src conversions for given fs_in and fs_out
%
% sof_src_generate(fs_in, fs_out <, fs_inout, <cfg>>)
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
%   gain    - overal filter gain, defaults to -1 dB if empty
%
% If fs_inout matrix is omitted this script will compute coefficients
% for all fs_in <-> fs_out combinations.
%
% If cfg is omitted the script will assume 'int32', 'std', 1.0, 0.
%

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2016-2024, Intel Corporation. All rights reserved.
%
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

src_paths(1);

if (nargin < 2) || (nargin > 4)
	error('Incorrect arguments for function!');
end
if nargin < 4
	cfg.ctype = 'int32';
	cfg.profile = 'std';
	cfg.quality = 1.0;
	cfg.speed = 0;
	cfg.gain = -1;
	cfg.thdn = -90;
end
if nargin < 3
	fs_inout = ones(length(fs_in), length(fs_out));
end

sio = size(fs_inout);
if (length(fs_in) ~= sio(1)) ||  (length(fs_out) ~= sio(2))
	error('Sample rates in/out matrix size mismatch!');
end

if exist('OCTAVE_VERSION', 'builtin')
  pkg load signal
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
tbl.data = [];
tbl.idx = 0;
for pass = 1:2
	for b = 1:nfso
		for a = 1:nfsi
			fs1 = fs_in(a);
			fs2 = fs_out(b);
			if fs_inout(a,b) < eps
				continue;
			end

			[l1, m1, l2, m2] = src_factor2_lm(fs1, fs2);
			% If the interpolate/decimate factors are low, use single step
			% conversion. The increases RAM consumption but lowers the MCPS.
			if cfg.speed && max(l1 * l2, m1 * m2) < 30
				l1 = l1 * l2;
				l2 = 1;
				m1 = m1 * m2;
				m2 = 1;
			end
			fs3 = fs1 * l1 / m1;
			cnv1 = src_param(fs1, fs3, coef_bits, cfg.quality, cfg.gain);
			cnv2 = src_param(fs3, fs2, coef_bits, cfg.quality, cfg.gain);
			if (fs2 < fs1)
				% When decimating 1st stage passband can be limited
				% for wider transition band
				f_pb = fs2 * cnv2.c_pb;
				cnv1.c_pb = f_pb / min(fs1, fs3);
			end
			if (fs2 > fs1)
				% When interpolating 2nd stage passband can be limited
				% for wider transition band
				f_pb = fs1 * cnv1.c_pb;
				cnv2.c_pb = f_pb / min(fs2, fs3);
			end
			if pass == 1
				src1 = src_get(cnv1);
				src2 = src_get(cnv2);
				delta = test_proto_src(src1, src2, cfg);
				big_step = 0;
				if delta > 0
					big_step = delta;
					fprintf(1, 'Increase attenuation by %.1f dB\n', big_step);
					cnv1.rs = cnv1.rs + big_step;
					cnv2.rs = cnv2.rs + big_step;
					src1 = src_get(cnv1);
					src2 = src_get(cnv2);
					delta = test_proto_src(src1, src2, cfg);
				end
				if big_step
					while delta < -1.5
						rs_step = -1;
						fprintf(1, 'Step attenuation by %.1f dB\n', rs_step);
						cnv1.rs = cnv1.rs + rs_step;
						cnv2.rs = cnv2.rs + rs_step;
						src1 = src_get(cnv1);
						src2 = src_get(cnv2);
						delta = test_proto_src(src1, src2, cfg);
					end
					while delta > 0
						rs_step = 1;
						fprintf(1, 'Increase attenuation by %.1f dB\n', rs_step);
						cnv1.rs = cnv1.rs + rs_step;
						cnv2.rs = cnv2.rs + rs_step;
						src1 = src_get(cnv1);
						src2 = src_get(cnv2);
						delta = test_proto_src(src1, src2, cfg);
					end
				end
				tbl = export_check(src1, tbl, coef_label, coef_ctype, hdir, cfg);
				tbl = export_check(src2, tbl, coef_label, coef_ctype, hdir, cfg);
			else
				src1 = export_get(l1, m1, cnv1, tbl);
				src2 = export_get(l2, m2, cnv2, tbl);
				fs2_check = fs1 * src1.L / src1.M * src2.L / src2.M;
				if abs(fs2 - fs2_check) > eps
					error('Something went wrong.');
				end
				k = gcd(src1.blk_out, src2.blk_in);
				stage1_times = src2.blk_in/k;
				stage2_times = stage1_times * src1.blk_out / src2.blk_in;
				defs.stage1_times_max = max(defs.stage1_times_max, stage1_times);
				defs.stage2_times_max = max(defs.stage2_times_max, stage2_times);
				l_2s(:,a,b) = [src1.L src2.L];
				m_2s(:,a,b) = [src1.M src2.M];
				mops_2s(:,a,b) = [src1.MOPS src2.MOPS];
				pb_2s(:,a,b) = [src1.c_pbi src2.c_pbi];
				sb_2s(:,a,b) = [src1.c_sbi src2.c_sbi];
				taps_2s(:,a,b) = [src1.filter_length src2.filter_length];
				defs.fir_delay_size = max(defs.fir_delay_size, src1.fir_delay_size);
				defs.out_delay_size = max(defs.out_delay_size, src1.out_delay_size);
				defs.blk_in = max(defs.blk_in, src1.blk_in);
				defs.blk_out = max(defs.blk_out, src1.blk_out);
				defs.fir_delay_size = max(defs.fir_delay_size, src2.fir_delay_size);
				defs.out_delay_size = max(defs.out_delay_size, src2.out_delay_size);
				defs.blk_in = max(defs.blk_in, src2.blk_in);
				defs.blk_out = max(defs.blk_out, src2.blk_out);
				defs.stage_buf_size = max(defs.stage_buf_size, src1.blk_out * stage1_times);
			end
		end % a
	end % b
end % pass

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

src_paths(0);

end

%% Helper functions

function tbl = export_check(src_in, tbl, coef_label, coef_ctype, hdir, cfg)
if src_in.active
	upgrade_converter = false;
	new_converter = true;
	item = [src_in.L src_in.M src_in.c_pbi src_in.c_sbi src_in.filter_length];
	for i = 1:tbl.idx
		if sum(abs(tbl.data(i, 1:4) - item(1:4))) < eps
			new_converter = false;
			fprintf(1, 'Conversion exists\n');
			previous_length = tbl.data(i,5);
			if src_in.filter_length > previous_length
				fprintf(1, 'Conversion is upgraded %d -> %d\n', ...
					previous_length, src_in.filter_length);
				upgrade_converter = true;
				tbl.data(i, :) = item;
				tbl.src(i) = src_in;
			end
		end
	end
	if new_converter || upgrade_converter
		src_export_coef(src_in, coef_label, coef_ctype, hdir, cfg.profile);
	end
	if new_converter
		tbl.idx = tbl.idx + 1;
		tbl.data(tbl.idx, :) = item;
		tbl.src(tbl.idx) = src_in;
	end
end
end

function src_out = export_get(l, m, cnv, tbl)

src_out.active = 0;
src_out.L=1;
src_out.M=1;
src_out.odm=1;
src_out.idm=1;
src_out.MOPS=0;
src_out.c_pb = 0;
src_out.c_sb = 0;
src_out.fir_delay_size = 0;
src_out.out_delay_size = 0;
src_out.blk_in = 1;
src_out.blk_out = 1;
src_out.gain = 1;
src_out.filter_length = 0;
src_out.c_pbi = 0;
src_out.c_sbi = 0;

if abs(cnv.fs1 - cnv.fs2) < 1
	return
end

pbi = round(1e4 * cnv.c_pb);
sbi = round(1e4 * cnv.c_sb);
spec = [l m  pbi sbi ];
for i = 1:tbl.idx
	if sum(abs(tbl.data(i, 1:4) - spec)) < eps
		src_out = tbl.src(i);
		return
	end
end
error('Not found, something went wrong');

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

function delta = test_proto_src(src1, src2, cfg)

if src1.filter_length < 1
	delta = 0;
	return;
end

[fs1, fs2] = src_fs12(src1, src2);
t = 1.0;
t_ignore = 0.5;
a = 10^(-1/20);
f_start = 100;
f_end = min(0.45 * min(fs1, fs2), 20e3);
n_oct = ceil(log(f_end / f_start) / log(2));
f = logspace(log10(f_start), log10(f_end), n_oct);
thdn = zeros(1, n_oct);
for i = 1:n_oct
	thdn(i) = src_thdn(src1, src2, f(i), a, t_ignore, t);
end

delta =  max(thdn) - cfg.thdn;

end

function [fs1, fs2] = src_fs12(src1, src2)

fs1 = src1.fs1;
if src2.filter_length > 0
	fs2 = src2.fs2;
else
	fs2 = src1.fs2;
end

end

function thdn = src_thdn(src1, src2, f, a, t_ignore, t)

[fs1, fs2] = src_fs12(src1, src2);
x = multitone(fs1, f, a, t);
y2 = proto_src(src1, src2, x);

% Apply standard low-pass for higher output rates
if fs2 > 42e3
	y2 = stdlpf(y2, 20e3, fs2);
end

y3 = stdnotch(y2, f, fs2);
idx = round(t_ignore * fs2);
level_total = 20*log10(a);
level_residual = level_dbfs(y3(idx:end));
thdn = level_residual - level_total;

end

function y2 = proto_src(src1, src2, x)

y1 = proto_src1(src1, x);
if src2.filter_length > 0
	y2 = proto_src1(src2, y1);
else
	y2 = y1;
end

end

function y = proto_src1(src, x)

if src.L > 1
	x0 = zeros(length(x) * src.L, 1);
	x0(1:src.L:end) = x;
else
	x0 = x;
end
y0 = filter(src.coefs, 1, x0) * src.gain;
if src.M > 1
	y = y0(1:src.M:end);
else
	y = y0;
end
end
