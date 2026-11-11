%% SPDX-License-Identifier: BSD-3-Clause
%%
%% Copyright(c) 2022 Intel Corporation. All rights reserved.

function ref_window()

	get_ref_window('rectangular', 'ref_window_rectangular.h', 16);
	get_ref_window('blackman', 'ref_window_blackman.h', 160);
	get_ref_window('hamming', 'ref_window_hamming.h', 256);
	get_ref_window('povey', 'ref_window_povey.h', 200);

end

function get_ref_window(wname, headerfn, wlength)

	switch lower(wname)
		case 'rectangular'
			win = boxcar(wlength);
		case 'blackman'
			win = mfcc_blackman(wlength);
		case 'hamming'
			win = hamming(wlength);
		case 'povey'
			win = mfcc_povey(wlength);
		otherwise
			fprintf(1, 'Illegal window name %s. ', wname);
			error('Failed.');
	end

	qwin = quant_s16(win);
	vn = sprintf('ref_%s', lower(wname));
	dn = sprintf('LENGTH_%s', upper(wname));
	export_headerfile(headerfn, vn, dn, qwin);
	fprintf(1, 'Exported %s\n', headerfn);

end

function y = quant_s16(x)

	scale = 2^15;
	intmax = scale - 1;
	intmin = -scale;

	y = max(min(round(x * scale), intmax), intmin);
end

function export_headerfile(headerfn, vn, dn, win)

	fh = fopen(headerfn, 'w');
	fprintf(fh, '/* SPDX-License-Identifier: BSD-3-Clause\n');
	fprintf(fh, ' *\n');
	fprintf(fh, ' * Copyright(c) %s Intel Corporation. All rights reserved.\n', ...
		datestr(now, 'yyyy'));
	fprintf(fh, ' */\n\n');
	fprintf(fh, '#define %s %d\n\n', dn, length(win));
	fprintf(fh, 'static const int16_t %s[%s] = {\n', vn, dn);

	for i = 1:length(win)
		fprintf(fh, '\t%d,\n', win(i));
	end

	fprintf(fh, '};\n');
	fclose(fh);

end
