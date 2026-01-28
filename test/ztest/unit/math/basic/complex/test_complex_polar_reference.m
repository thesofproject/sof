function test_complex_polar_reference()

    % Make a box flattened spiral of (re,im) values to exercise the
    % Q1.31 complex numbers range
    q31_scale = 2^31;
    num_points = 1000;
    magnitude0 = linspace(0, sqrt(2), num_points);
    angle0 = pi/4 + linspace(0, 10*2*pi, num_points);
    re = max(min(magnitude0 .* cos(angle0), 1), -1);
    im = max(min(magnitude0 .* sin(angle0), 1), -1);
    ref_re = int32(re * q31_scale);
    ref_im = int32(im * q31_scale);
    re = double(ref_re)/q31_scale;
    im =  double(ref_im)/q31_scale;
    figure(1)
    plot(re, im)
    grid on

    % In polar format magnitude is Q2.30 and angle Q3.29
    q29_scale = 2^29;
    q30_scale = 2^30;
    magnitude = sqrt(re.^2 + im.^2);
    phase = angle(complex(re, im));
    ref_magnitude = int32(magnitude * q30_scale);
    ref_angle = int32(phase * q29_scale);
    magnitude = double(ref_magnitude)/q30_scale;
    phase = double(ref_angle)/q29_scale;

    figure(2)
    subplot(2,1,1);
    plot(magnitude);
    grid on
    subplot(2,1,2);
    plot(phase);
    grid on

    fh = export_headerfile('test_complex_polar_tables.h');
    dn = 'TEST_COMPLEX_POLAR_NUM_POINTS';
    vl = 6;
    export_define(fh, dn, num_points);
    export_array(fh, 'test_real_values', dn, vl, ref_re);
    export_array(fh, 'test_imag_values', dn, vl, ref_im);
    export_array(fh, 'test_magnitude_values', dn, vl, ref_magnitude);
    export_array(fh, 'test_angle_values', dn, vl, ref_angle);
    fclose(fh);

end

function fh = export_headerfile(headerfn)
    fh = fopen(headerfn, 'w');
    fprintf(fh, '/* SPDX-License-Identifier: BSD-3-Clause\n');
    fprintf(fh, ' *\n');
    fprintf(fh, ' * Copyright(c) %s Intel Corporation.\n', ...
	    datestr(now, 'yyyy'));
    fprintf(fh, ' */\n\n');
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
