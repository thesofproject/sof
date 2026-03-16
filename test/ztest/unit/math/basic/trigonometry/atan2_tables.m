% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright(c) 2026 Intel Corporation.

function atan2_tables()

    % Create random test points with bias to min/max values
    q31_scale = 2^31;
    q29_scale = 2^29;
    num_points = 256;
    rand('seed', 42);
    x = max(min(2.2 * rand(num_points, 1) - 1.1, 1), -1);
    y = max(min(2.2 * rand(num_points, 1) - 1.1, 1), -1);

    % Force 1st test point to be (0, 0) to ensure it is tested
    x(1) = 0;
    y(1) = 0;

    % Then force 10 test points to have x = 0, the reference values are 0 or pi
    x(2:11) = 0;

    % And force 10 test points to have y = 0, the reference values are +/-pi
    y(12:21) = 0;

    % Quantize input values
    ref_x = int32(x * q31_scale);
    ref_y = int32(y * q31_scale);
    x = double(ref_x)/q31_scale;
    y = double(ref_y)/q31_scale;
    a = atan2(y, x);
    ref_a = int32(a * q29_scale);

    figure(1)
    subplot(2,1,1)
    plot(x, y, 'o');
    grid on;
    subplot(2,1,2)
    plot(a, 'o');
    grid on;

    fn = 'atan2_tables.h';
    fh = export_headerfile(fn);
    dn = 'ATAN2_TEST_TABLE_SIZE';
    vl = 6;
    fu = export_ifndef(fh, fn);
    export_define(fh, dn, num_points);
    export_array(fh, 'atan2_test_y', dn, vl, ref_y);
    export_array(fh, 'atan2_test_x', dn, vl, ref_x);
    export_array(fh, 'atan2_test_ref', dn, vl, ref_a);
    export_fclose(fh, fu);

end

function [fh] = export_headerfile(headerfn)
    fh = fopen(headerfn, 'w');
    fprintf(fh, '/* SPDX-License-Identifier: BSD-3-Clause\n');
    fprintf(fh, ' *\n');
    fprintf(fh, ' * Copyright(c) %s Intel Corporation.\n', ...
	    datestr(now, 'yyyy'));
    fprintf(fh, ' */\n\n');
end

function fu = export_ifndef(fh, fn)
    fu = sprintf('__%s__', upper(strrep(fn, '.', '_')));
    fprintf(fh, '#ifndef %s\n', fu);
    fprintf(fh, '#define %s\n\n', fu);
    fprintf(fh, '#include <stdint.h>\n\n');
end

function export_define(fh, dn, val)
    fprintf(fh, '#define %s %d\n', dn, val);
end

function export_array(fh, vn, size_str, vl, data)
    fprintf(fh, '\n');
    fprintf(fh, 'static const int32_t %s[%s] = {\n', vn, size_str);

    n = length(data);
    k = 0;
    for i = 1:vl:n
        fprintf(fh, '\t');
        for j = 1:min(vl, n-k)
            k = k + 1;
	    fprintf(fh, '%12d,', data(k));
        end
        fprintf(fh, '\n');
    end

    fprintf(fh, '};\n');
end

function export_fclose(fh, fu)
    fprintf(fh, "\n#endif /* %s */\n", fu);
    fclose(fh);
end
