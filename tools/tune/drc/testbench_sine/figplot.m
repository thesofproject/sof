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
function figplot(y)
clf();
figure(1)
x = fi((-pi*1/pi:0.1:pi*1/pi),1,32,30);
subplot(2,2,1:2);
plot(x,y,'b-x'); grid on;
xlabel('fix(-3\pi) < x <fix( 3\pi)');ylabel('y = sin(x)');legend({'y = sin(x)'},'Location','best')
title('sin(x)-floatpt');

subplot(2,2,3);
polarplot(((-pi*1/pi:0.1:pi*1/pi)),y)
pax = gca;
pax.ThetaAxisUnits = 'Degree';
pax = gca;
pax.ThetaColor = 'magenta';
pax.RColor = [0 .5 0];
pax.GridColor = 'black';

title('Degree-sin(x)-floatpt');

subplot(2,2,4);
polarplot((deg2rad(-pi*1/pi:0.1:pi*1/pi)),y)
pax = gca;
pax.ThetaAxisUnits = 'radians';
pax = gca;
pax.ThetaColor = 'blue';
pax.RColor = [0 .5 0];
pax.GridColor = 'red';
title('Radians-sin(x)-floatpt');

