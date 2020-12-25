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
clf();
figNo = 100;

for k = 1:length(Files)
    [~,foldername]   = fileparts(Files(k).folder);
    [~,filename,ext] = fileparts(Files(k).name);
    disp([foldername,    filename,    ext]);
    pattern = ["Results",];
    extn    = ["",'.txt'];
    % prepare for data plots

    if contains(foldername,pattern) && contains(filename,'mag2dB') && contains(ext,extn)  && Files(k).bytes > 1
        figure(figNo+1);
        mag2dB = get_drc_mag2db_fixed(Files(k).folder, Files(k).name);
        % prepare for data plots
        idx             = mag2dB.idx;
        testvector      = mag2dB.testvector/2.^26;
        Fixlog10linear  = mag2dB.Fixlog10linear/2.^21;

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

    end
    if contains(foldername,pattern) && contains(filename,'drc_sin_fixed') && contains(ext,extn)  && Files(k).bytes > 1
        figure(figNo+2);
        drcsinfixed = get_drc_sine_fixed(Files(k).folder, Files(k).name);
        idx         = drcsinfixed.idx;
        fixptvector = drcsinfixed.insine/2.^30;    % in Radian
        fixptsine   = drcsinfixed.outsine/2.^31;


        % Plot output 1
        subplot(3,2,1);
        plot(idx, fixptvector,'DisplayName','input-range');grid on;
        xlabel('fix(-3\pi) < x <fix( 3\pi)');ylabel('y = sin(x)');legend({'y = sin(x)'},'Location','best')
        title('Range-Sin[Q2.30]');

        % Plot output 2
        subplot(3,2,2);
        plot(idx, fixptsine,'r-+','DisplayName','Johny-sine');grid on;
        xlabel('fix(-3\pi) < x <fix( 3\pi)');ylabel('y = sin(x)');legend({'y = sin(x)'},'Location','best')
        title( 'Johny-Fixpt-drc-Sin-fixed[Q1.31]');
        % Plot output 3
        subplot(3,2,3);
        polarplot(fixptvector,max(0,fixptsine),'+-r')
        pax= gca;
        pax.FontSize = 12;
        title('polar-sin(x*pi/2)-Johny-Fixpt-drc-Sin-fixed[Q1.31]');
    end

    if contains(foldername,pattern) && contains(filename,'sin_fixed') && contains(ext,extn)  && Files(k).bytes > 1
        figure(figNo+2);
        refsinfixed = get_ref_sine_fixed(Files(k).folder, Files(k).name);
        Index         = refsinfixed.Index;
        InvalXq230    = refsinfixed.InvalXq230/2.^30;
        OutvalYq115   = refsinfixed.OutvalYq115/2.^15;


        % Plot output 4
        subplot(3,2,4);
        x = fi((-1:0.1:1),1,32,30);
        plot(Index,sin(x*pi/2),'g-+','DisplayName','sine');grid on;
        xlabel('fix(-3\pi) < x <fix( 3\pi)');ylabel('y = sin(x * pi/2)');legend({'y = sin(x * pi/2)'},'Location','best')
        title( 'Ref-Sin-Floatpt');
        % Plot output 5
        subplot(3,2,5);
        plot(Index,OutvalYq115,'b-+','DisplayName','Shriram-sine');grid on;
        xlabel('fix(-3\pi) < x <fix( 3\pi)');ylabel('y = sin(x * pi/2)');legend({'y = sin(x * pi/2)'},'Location','best')
        title( 'Sriram-Fixpt-fixpt-ccode-Sin[Q1.15]');
        % Plot output 6
        subplot(3,2,6);
        polarplot(InvalXq230,max(0,OutvalYq115),'*-g')
        pax = gca;
        pax.ThetaAxisUnits = 'Degree';
        pax = gca;
        pax.ThetaColor = 'green';
        pax.RColor = [0 .7 0];
        pax.GridColor = 'black';
        title('polar-sin(x*pi/2)-Sriram-Fixpt-fixpt-ccode-Sin[Q1.15]');
    end
    if contains(foldername,pattern) && contains(filename,'asin_fixed') && contains(ext,extn)  && Files(k).bytes > 1
        figure(figNo+3);
        refasinfixed = get_ref_asine_fixed(Files(k).folder, Files(k).name);
        Index         = refasinfixed.Index;
        InvalXq230    = refasinfixed.InvalXq230/2.^30;
        OutvalYq131   = refasinfixed.OutvalYq131/2.^31;


        % Plot output 1
        subplot(3,2,1:2);
        x = fi((-1:0.1:1),1,32,30);
        plot(Index,2/pi * asin(x),'g-+','DisplayName','sine');grid on;
        xlabel('fix(-3\pi) < x <fix( 3\pi)');ylabel('y = 2/pi*asin(x)');legend({'y = 2/pi*asin(x)'},'Location','best')
        title( 'Ref-ASin-Floatpt');
        % Plot output 2
        subplot(3,2,3:4);
        plot(Index,OutvalYq131,'b-+','DisplayName','Shriram-sine');grid on;
        xlabel('fix(-3\pi) < x <fix( 3\pi)');ylabel('y = 2/pi*asin(x)');legend({'y = 2/pi*asin(x)'},'Location','best')
        title( 'Sriram-Fixpt-fixpt-ccode-ASin[Q1.31]');
        % Plot output 3
        subplot(3,2,5);
        polarplot(InvalXq230,OutvalYq131)
        pax = gca;
        pax.ThetaAxisUnits = 'Degree';
        pax = gca;
        pax.ThetaColor = 'magenta';
        pax.RColor = [0 .5 0];
        pax.GridColor = 'black';
        title('Degree-asin(x)-fixpt');
        % Plot output 4
        subplot(3,2,6);
        polarplot(deg2rad(InvalXq230),OutvalYq131);
        pax = gca;
        pax.ThetaAxisUnits = 'radians';
        pax = gca;
        pax.ThetaColor = 'blue';
        pax.RColor = [0 .5 0];
        pax.GridColor = 'red';
        title('Radians-asin(x)-fixpt');
    end
    if contains(foldername,pattern) && contains(filename,'drc_asin_fixed') && contains(ext,extn)  && Files(k).bytes > 1
        figure(figNo+4);
        drcasinfixed = get_drc_asine_fixed(Files(k).folder, Files(k).name);
        Index         = drcasinfixed.idx;
        InvalXq230    = drcasinfixed.inasine/2.^30;
        OutvalYq130   = drcasinfixed.outasine/2.^30;


        % Plot output 1
        subplot(3,2,1:2);
        x = fi((-1:0.1:1),1,32,30);
        plot(Index,2/pi * asin(x),'g-+','DisplayName','sine');grid on;
        xlabel('fix(-3\pi) < x <fix( 3\pi)');ylabel('y = 2/pi*asin(x)');legend({'y = sin(x)'},'Location','best')
        title( 'drc-ASin-Floatpt');
        % Plot output 2
        subplot(3,2,3:4);
        plot(Index,OutvalYq130,'b-+','DisplayName','Shriram-sine');grid on;
        xlabel('fix(-3\pi) < x <fix( 3\pi)');ylabel('y = 2/pi*asin(x)');legend({'y = 2/pi*asin(x)'},'Location','best')
        title( 'drc-fixpt-ccode-ASin[Q2.30]');
        % Plot output 3
        subplot(3,2,5);
        polarplot(InvalXq230,OutvalYq130)
        pax = gca;
        pax.ThetaAxisUnits = 'Degree';
        pax = gca;
        pax.ThetaColor = 'blue';
        pax.RColor = [0 .5 0];
        pax.GridColor = 'black';
        title('Degree-asin(x)-fixpt');
        % Plot output 4
        subplot(3,2,6);
        polarplot(deg2rad(InvalXq230),OutvalYq130);
        pax = gca;
        pax.ThetaAxisUnits = 'radians';
        pax = gca;
        pax.ThetaColor = 'magenta';
        pax.RColor = [0 .5 0];
        pax.GridColor = 'red';
        title('Radians-asin(x)-fixpt');
    end
end
