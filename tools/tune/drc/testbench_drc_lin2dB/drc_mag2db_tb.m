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
clearvars;clc;close all;
x =  fi((-31.9:0.1:31.9),1,32,26);
y = drc_mag2db(data_initialization);
Numpt = 1:length(y);
clf();
figure(100);
subplot(3,1,1);
plot(Numpt,y,'r-x',Numpt,x,'b-o');grid on;
legend('fixpt-log[ydB]','Linear[Q.6.26]','location','best');
xlabel('Numpt[X-axis]');ylabel('mag[dB');title('linear[Q6.26]Decibel[Q11.21]');
subplot(3,1,2);
plot(Numpt,real(y),'m-+',Numpt,imag(y),'y+',Numpt,20*sign((-31.9:0.1:31.9)).*log10(abs((-31.9:0.1:31.9))+ 1i*angle((-31.9:0.1:31.9))),'g-o');grid on;
legend('fixpt-Relog[ydB]','fixpt-Imlog[ydB]','floatpt-log[dB]','location','best');
xlabel('Numpt[X-axis]');ylabel('mag[dB');title('linear2decibel[sign]');
subplot(3,1,3);
plot(Numpt,real(y),'r-x',Numpt,imag(y),'y+',Numpt,20*log(abs((-31.9:0.1:31.9))+ 1i*angle((-31.9:0.1:31.9)))/log(10),'c-x');hold on; grid on;
legend('fixpt-Relog[ydB]','fixpt-Imlog[ydB]','floatpt-log[dB]','location','best');
xlabel('Numpt[X-axis]');ylabel('mag[dB');title('linear2decibel[wo/sign]');
