%% Create DMIC FIR configuration files

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2019, Intel Corporation. All rights reserved.
%
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

%% Generic parameters
prm.driver_abi_version = 1;
prm.duty_min = 40;
prm.duty_max = 60;
prm.fifo_a_fs = 0;
prm.fifo_a_bits = 24;
prm.fifo_b_fs = 0;
prm.fifo_b_bits = 24;
prm.number_of_pdm_controllers = 2;
prm.pdm(1).enable_mic_a = 1;
prm.pdm(1).enable_mic_b = 1;
prm.pdm(1).polarity_mic_a = 0;
prm.pdm(1).polarity_mic_b = 0;
prm.pdm(1).clk_edge = 0;
prm.pdm(1).skew = 0;
prm.pdm(2).enable_mic_a = 1;
prm.pdm(2).enable_mic_b = 1;
prm.pdm(2).polarity_mic_a = 0;
prm.pdm(2).polarity_mic_b = 0;
prm.pdm(2).clk_edge = 0;
prm.pdm(2).skew = 0;

%% Design
prm.pdmclk_min = 2.4e6;
prm.pdmclk_max = 4.8e6;

% clkdiv, mcic, mfira, mfirb
prm.fifo_a_fs =  8e3; prm.fifo_b_fs = 16e3; dmic_init(prm); %  8, 30, 10, 5
prm.fifo_a_fs =  8e3; prm.fifo_b_fs = 32e3; dmic_init(prm); %  8, 25, 12, 3
prm.fifo_a_fs =  8e3; prm.fifo_b_fs = 48e3; dmic_init(prm); %  8, 25, 12, 2
prm.fifo_a_fs = 24e3; prm.fifo_b_fs = 96e3; dmic_init(prm); %  5, 20,  8, 2
prm.fifo_a_fs = 32e3; prm.fifo_b_fs = 96e3; dmic_init(prm); %  5, 20,  6, 2
prm.fifo_a_fs = 48e3; prm.fifo_b_fs = 96e3; dmic_init(prm); %  5, 20,  4, 2
prm.fifo_a_fs = 48e3; prm.fifo_b_fs = 16e3; dmic_init(prm); %  8, 25,  2, 6

%% No need to run due to duplicated FIR coefficients
%prm.fifo_a_fs =  8e3; prm.fifo_b_fs = 24e3; dmic_init(prm); %  8, 25, 12, 4
%prm.fifo_a_fs = 16e3; prm.fifo_b_fs = 32e3; dmic_init(prm); %  8, 25,  6, 3
%prm.fifo_a_fs = 16e3; prm.fifo_b_fs = 24e3; dmic_init(prm); %  8, 25,  6, 4
%prm.fifo_a_fs = 16e3; prm.fifo_b_fs = 96e3; dmic_init(prm); %  5, 20, 12, 2
%prm.fifo_a_fs = 24e3; prm.fifo_b_fs = 32e3; dmic_init(prm); %  8, 25,  4, 3
%prm.fifo_a_fs = 24e3; prm.fifo_b_fs = 48e3; dmic_init(prm); %  8, 25,  4, 2
%prm.fifo_a_fs = 32e3; prm.fifo_b_fs = 48e3; dmic_init(prm); %  5, 20,  6, 4
