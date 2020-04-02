% print_fr(comp, filename, prm_in_list, prm_out_list, fr_db, fr_hz, pf)
%
% Prints and exports in CSV format a matrix of frequency response widths in Hz

% SPDX-License-Identifier: BSD-3-Clause
% Copyright(c) 2020 Intel Corporation. All rights reserved.
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

function print_fr(comp, fn, prm_in_list, prm_out_list, fr_db, fr_hz, pf)

n_prmi = length(prm_in_list);
n_prmo = length(prm_out_list);
fh = fopen(fn,'w');
fprintf(fh,'\n');
fprintf(fh,'%s test result: Frequency response +/- X.XX dB (YY.Y kHz) \n', comp);
fprintf(fh,'%8s, ', 'in \ out');

% Do not print decimals for all integer parameters
if isequal(floor(prm_in_list), prm_in_list) && isequal(floor(prm_out_list), prm_out_list)
	int_prms = 1;
else
	int_prms = 0;
end

for a = 1:n_prmo-1
	if int_prms
		fprintf(fh,'%12d, ', prm_out_list(a));
	else
		fprintf(fh,'%12.1f, ', prm_out_list(a));
	end
end

if int_prms
	fprintf(fh,'%12d', prm_out_list(n_prmo));
else
	fprintf(fh,'%12.1f', prm_out_list(n_prmo));
end

fprintf(fh,'\n');
for a = 1:n_prmi
	if int_prms
		fprintf(fh,'%8d, ', prm_in_list(a));
	else
		fprintf(fh,'%8.1f, ', prm_in_list(a));
	end

	for b = 1:n_prmo
                if pf(a,b,1) < 0
                        cstr = 'x';
                else
                        cstr = sprintf('%4.2f (%4.1f)', fr_db(a,b), fr_hz(a,b,2)/1e3);
                end
                if b < n_prmo
                        fprintf(fh,'%12s, ', cstr);
                else
                        fprintf(fh,'%12s', cstr);
                end
        end
        fprintf(fh,'\n');
end

fclose(fh);
type(fn);
end
