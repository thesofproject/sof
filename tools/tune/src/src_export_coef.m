function success=src_export_coef(src, ctype, vtype, hdir, profile)

% src_export_coef - Export FIR coefficients
%
% success=src_export_coef(src, ctype, hdir, profile)
%
% src     - src definition struct
% ctype   - 'float','int32', or 'int24'
% vtype   - 'float','int32_t'
% hdir    - directory for header files
% profile - string to append to filename
%

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2016-2020, Intel Corporation. All rights reserved.
%
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

if nargin < 5
        profile = '';
end

if src.L == src.M
        success = 0;
else
        pbi =  round(src.c_pb*1e4);
        sbi =  round(src.c_sb*1e4);
        if isempty(profile)
                hfn = sprintf('%s/src_%s_%d_%d_%d_%d.h', ...
                        hdir, ctype, src.L, src.M, pbi, sbi);
        else
                hfn = sprintf('%s/src_%s_%s_%d_%d_%d_%d.h', ...
                        hdir, profile, ctype, src.L, src.M, pbi, sbi);
        end
        vfn = sprintf('src_%s_%d_%d_%d_%d_fir', ctype, src.L, src.M, pbi, sbi);
        sfn = sprintf('src_%s_%d_%d_%d_%d', ctype, src.L, src.M, pbi, sbi);

        fprintf('Exporting %s ...\n', hfn);
        fh = fopen(hfn, 'w');
	y = datestr(now(), 'yyyy');

	fprintf(fh, '/* SPDX-License-Identifier: BSD-3-Clause\n');
	fprintf(fh, ' *\n');
	fprintf(fh, ' * Copyright(c) %s Intel Corporation. All rights reserved.\n', y);
	fprintf(fh, ' *\n');
	fprintf(fh, ' */\n');
	fprintf(fh, '\n');
	fprintf(fh, '/** \\cond GENERATED_BY_TOOLS_TUNE_SRC */\n');
	fprintf(fh, '#include <sof/audio/src/src.h>\n');
	fprintf(fh, '#include <stdint.h>\n');
	fprintf(fh, '\n');

        switch ctype
                case 'float'
                        fprintf(fh, 'const %s %s[%d] = {\n', ...
                                vtype, vfn, src.filter_length);
                        fprintf(fh,'\t%16.9e', src.coefs(1));
                        for n=2:src.filter_length
                                fprintf(fh, ',\n');
                                fprintf(fh,'\t%16.9e', src.coefs(n));
                        end
                        fprintf(fh,'\n\n};');
                case 'int32'
			print_int_coef(src, fh, vtype, vfn, 32);
                case 'int24'
			print_int_coef(src, fh, vtype, vfn, 24);
                case 'int16'
			print_int_coef(src, fh, vtype, vfn, 16);
                otherwise
                        error('Unknown type %s !!!', ctype);
        end

        fprintf(fh, '\n');
        switch ctype
                case 'float'
                        fprintf(fh, 'struct src_stage %s = {\n', sfn);
			fprintf(fh, '\t%d, %d, %d, %d, %d, %d, %d, %d, %f,\n\t%s};\n', ...
                                src.idm, src.odm, src.num_of_subfilters, ...
                                src.subfilter_length, src.filter_length, ...
                                src.blk_in, src.blk_out, src.halfband, ...
                                src.gain, vfn);
                case { 'int16' 'int24' 'int32' }
                        fprintf(fh, 'struct src_stage %s = {\n', sfn);
			fprintf(fh, '\t%d, %d, %d, %d, %d, %d, %d, %d, %d,\n\t%s};\n', ...
                                src.idm, src.odm, src.num_of_subfilters, ...
                                src.subfilter_length, src.filter_length, ...
                                src.blk_in, src.blk_out, src.halfband, ...
                                src.shift, vfn);
                otherwise
                        error('Unknown type %s !!!', ctype);
        end
        fprintf(fh, '/** \\endcond */\n');
        fclose(fh);
        success = 1;
end

end

function print_int_coef(src, fh, vtype, vfn, nbits)
        fprintf(fh, 'const %s %s[%d] = {\n', ...
                vtype, vfn, src.filter_length);

        cint = coef_quant(src, nbits);
        fprintf(fh,'\t%d', cint(1));
        for n=2:src.filter_length
                fprintf(fh, ',\n');
                fprintf(fh,'\t%d', cint(n));
        end
        fprintf(fh,'\n\n};\n');
end

function cint = coef_quant(src, nbits)

	sref = 2^(nbits-1);
	pmax = sref-1;
	nmin = -sref;

        if nbits > 16
                %% Round() is OK
                cint0 = round(sref*src.coefs);
        else
                %% Prepare to optimize coefficient quantization
                fs = max(src.fs1, src.fs2);
                f = linspace(0, fs/2, 1000);

                %% Test sensitivity for stopband and find the most sensitive
                %  coefficients
                sbf = linspace(src.f_sb,fs/2, 500);
                n = src.filter_length;
                psens = zeros(1,n);
                bq0 = round(sref*src.coefs);
                h = freqz(bq0/sref/src.L, 1, sbf, fs);
                sb1 = 20*log10(sqrt(sum(h.*conj(h))));
                for i=1:n
                        bq = src.coefs;
                        bq(i) = round(sref*bq(i))/sref;
                        %tbq = bq; %tbq(i) = bq(i)+1;
                        h = freqz(bq, 1, sbf, fs);
                        psens(i) = sum(h.*conj(h));
                end
                [spsens, pidx] = sort(psens, 'descend');

                %% Test quantization in the found order
                %  The impact to passband is minimal so it's not tested
                bi = round(sref*src.coefs);
                bi0 = bi;
                dl = -1:1;
                nd = length(dl);
                msb = zeros(1,nd);
                for i=pidx
                        bit = bi;
                        for j=1:nd
                                bit(i) = bi(i) + dl(j);
                                h = freqz(bit, 1, sbf, fs);
                                msb(j) = sum(h.*conj(h));
                        end
                        idx = find(msb == min(msb), 1, 'first');
                        bi(i) = bi(i) + dl(idx);
                end
                h = freqz(bi/sref/src.L, 1, sbf, fs);
                sb2 = 20*log10(sqrt(sum(h.*conj(h))));

                %% Plot to compare
                if 0
                        f = linspace(0, fs/2, 1000);
                        h1 = freqz(src.coefs/src.L, 1, f, fs);
                        h2 = freqz(bq0/sref/src.L, 1, f, fs);
                        h3 = freqz(bi/sref/src.L, 1, f, fs);
                        figure;
                        plot(f, 20*log10(abs(h1)), f, 20*log10(abs(h2)), f, 20*log10(abs(h3)));
                        grid on;
                        fprintf('Original = %4.1f dB, optimized = %4.1f dB, delta = %4.1f dB\n', ...
                                sb1, sb2, sb1-sb2);
                end
                cint0 = bi;
        end


        %% Re-order coefficients for filter implementation
        cint = zeros(src.filter_length,1);
        for n = 1:src.num_of_subfilters
                i11 = (n-1)*src.subfilter_length+1;
                i12 = i11+src.subfilter_length-1;
                cint(i11:i12) = cint0(n:src.num_of_subfilters:end);
        end

        %% Done check for no overflow
        max_fix = max(cint);
	min_fix = min(cint);
	if (max_fix > pmax)
		printf('Fixed point coefficient %d exceeded %d\n.', max_fix, pmax);
		error('Something went wrong!');
	end
	if (min_fix < nmin)
		printf('Fixed point coefficient %d exceeded %d\n.', min_fix, nmax);
		error('Something went wrong!');
        end


end
