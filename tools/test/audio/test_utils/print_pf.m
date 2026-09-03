% print_pf(comp, filename, prm_in_list, prm_out_list, pf, desc)
%
% Prints and exports in CSV format a matrix of overall pass/fail test results

% SPDX-License-Identifier: BSD-3-Clause
% Copyright(c) 2020 Intel Corporation. All rights reserved.
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

function print_pf(comp, fn, prm_in_list, prm_out_list, pf, desc)

n_prmi = length(prm_in_list);
n_prmo = length(prm_out_list);
fh = fopen(fn,'w');
fprintf(fh,'\n');
fprintf(fh,'%s test result: %s\n', comp, desc);
fprintf(fh,'%8s, ', 'in \ out');

% Do not print decimals for all integer parameters
if isequal(floor(prm_in_list), prm_in_list) && isequal(floor(prm_out_list), prm_out_list)
	int_prms = 1;
else
	int_prms = 0;
end

for a = 1:n_prmo-1
	if int_prms
		fprintf(fh,'%14d, ', prm_out_list(a));
	else
		fprintf(fh,'%14.1f, ', prm_out_list(a));
	end
end

if int_prms
	fprintf(fh,'%14d', prm_out_list(n_prmo));
else
	fprintf(fh,'%14.1f', prm_out_list(n_prmo));
end

fprintf(fh,'\n');
spf = size(pf);
npf = spf(3);
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
                        cstr = sprintf('%d', pf(a,b,1));
                        for n=2:npf
                                if pf(a,b,n) < 0
                                        cstr = sprintf('%s/x', cstr);
                                else
                                        cstr = sprintf('%s/%d', cstr,pf(a,b,n));
                                end
                        end
                end
                if b < n_prmo
                        fprintf(fh,'%14s, ', cstr);
                else
                        fprintf(fh,'%14s', cstr);
                end
        end
        fprintf(fh,'\n');
end

fclose(fh);
type(fn);
end
