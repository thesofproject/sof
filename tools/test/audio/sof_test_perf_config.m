% SPDX-License-Identifier: BSD-3-Clause
% Copyright(c) 2019 Intel Corporation. All rights reserved.

%%
%% Configuration for a playback test
%%

%% Set user@domain for ssh, need to be set for playback test
play.user = 'user@host.domain';

%% Other playback settings
play.ssh = 1;			% Set to 1 for for remote play
play.dir = '/tmp';             	% directory for temporary files
play.dev = 'hw:0,0'; 		% Audio device for playback
play.sft = 'S16_LE'; 		% Sample format to use
play.nch = 2;         		% Number of playback channels
play.ch  = [1 2];		% Playback channels to test

%% Set to user@domain for ssh, not set for playback test
rec.user = '';

%% Other capture settings
rec.ssh = 0;			% Set to 1 for remote capture
rec.dir = '/tmp'; 		% Directory for temporary files
rec.dev = 'hw:1,0'; 		% Audio capture device
rec.sft = 'S24_3LE'; 		% Sample format to use
rec.nch = 8; 			% Number audio capture channels
rec.ch  = [1 2];		% Capture channels to measure

%% Generic settings
test.id = 'Example';            % Label for test
test.fs = 48000;		% Sample rate
test.quick = 1; 		% Speed up some test cases
test.att_rec_db = 3; 		% Attenuation assumption for capture

%% Gain test case
test.g_db_tol = 0.5;		% Tolerance for gain
test.g_db_expect = 0.0;         % Expected gain

%% Frequency response test case, for 20 - 20000 Hz
test.fr_rp_max_db = 1.0;	     % Upper limit for p-p ripple in dB
test.plot_fr_axis = [10 30e3 -4 1];  % Plot xmin, xmax, ymin, ymax

%% THD+N test case
test.thdnf_max = -55;			   % Upper limit for THD+N
test.plot_thdn_axis = [10 30e3 -90 -40]; % Plot xmin xmax ymin ymax
