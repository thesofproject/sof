%  SPDX-License-Identifier: BSD-3-Clause
%
%  Copyright(c) 2020 Intel Corporation. All rights reserved.
%
%  Author: Shriram Shastry <malladi.sastry@linux.intel.com>
%---------------------------------------------------
%---------------------------------------
%   History
%---------------------------------------
%   2020/12/24 Sriram Shastry       - initial version
%
WorkingDir = uigetdir();
Files      = subdir(WorkingDir);
for k = 1:length(Files)
    [~,foldername]   = fileparts(Files(k).folder);
    [~,filename,ext] = fileparts(Files(k).name);
    disp([foldername,    filename,    ext]);
    pattern = ["Results",];
    extn    = ["",'.txt'];
    if contains(foldername,pattern) && contains(filename,'file') && contains(ext,extn)  && Files(k).bytes > 1
        file = get_drc_log10_fixed(Files(k).folder, Files(k).name);
    end
end
% prepare for data plots
idx             = file.idx;
testvector      = file.testvector/2.^26;
Fixlog10linear  = file.Fixlog10linear/2.^21;

figNo = 100;
clf();
figure(figNo+1);
% Plot input 1
subplot(3,2,1);
plot(idx, testvector,'DisplayName','linear')
ylabel('mag[dB]');xlabel('Numpts');grid on;
title('linear[Q6.26]');

% Plot output 2
subplot(3,2,2);
plot(idx, Fixlog10linear,'DisplayName','decibel')
ylabel('mag[dB]');xlabel('Numpts');grid on;
title( 'C-FixedptOut-drc-lin2db-fixed[Q11.21]');

% Plot output 3
subplot(3,2,3:4);
testvector(isnan(testvector)) = [];
plot(idx,20*log10(abs(testvector)),'DisplayName','decibel');
ylabel('mag[dB]');xlabel('Numpts');grid on;
title( 'Reference-Floatpt');

% Plot output 4
subplot(3,2,5:6);
plot(idx, Fixlog10linear,idx,20*log(abs(testvector))/log(10),'DisplayName','error-decibel')
ylabel('mag[dB]');xlabel('Numpts');grid on;legend('fixtpt','floatpt','location','best');
title( 'Error-decibel[fixpt-floatpt]');


