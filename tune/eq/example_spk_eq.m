%% Design an example speaker equalizer with the provided sample data
%  This equalizer by default uses a FIR and IIR components those should be
%  both in the speaker pipeline.
%
%  Note that IIR should be first since the included band-pass response provides
%  signal headroom.

%%
% Copyright (c) 2016, Intel Corporation
% All rights reserved.
%
% Redistribution and use in source and binary forms, with or without
% modification, are permitted provided that the following conditions are met:
%   * Redistributions of source code must retain the above copyright
%     notice, this list of conditions and the following disclaimer.
%   * Redistributions in binary form must reproduce the above copyright
%     notice, this list of conditions and the following disclaimer in the
%     documentation and/or other materials provided with the distribution.
%   * Neither the name of the Intel Corporation nor the
%     names of its contributors may be used to endorse or promote products
%     derived from this software without specific prior written permission.
%
% THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
% AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
% IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
% ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
% LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
% CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
% SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
% INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
% CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
% ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
% POSSIBILITY OF SUCH DAMAGE.
%
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
%

%% Get defaults for equalizer design
eq = eq_defaults();

%% Settings for this EQ
eq.fs = 48e3;               % Default sample rate in SOF
eq.enable_fir = 1;          % Try enabling and disabling FIR part
eq.enable_iir = 1;          % Try enabling and disabling IIR part
eq.norm_type = 'loudness';  % Preserve loudness approximately
eq.norm_offs_db = 0;        % Can be used to control gain
eq.p_fmin = 100;            % With this data start plots from 100 Hz
eq.p_fmax = 20e3;           % and end to 20 kHz.

%% Get acousticial frequency response measurement data. This is
%  a quite typical response for a miniature speaker. Alternatively
%  the response could be read from spreadsheet column format that is
%  commonly supported by audio measurement equipment. Then extract
%  the frequency and magnitude columns. E.g.
%
%      data = xlsread('spkr.xlsx', 1, 'A5:B78');
%      eq.raw_f = data(:,1);
%      eq.raw_m_db = data(:,2);

eq.raw_f = [ ...
         298.533,  316.232,  334.971,  354.813,  375.838,  398.106,  ...
         421.699,  446.685,  473.153,  501.186,  530.887,  562.342,  ...
         595.663,  630.958,  668.345,  707.946,  749.895,  794.329,  ...
         841.397,  891.252,  944.062,     1000,  1059.26,  1122.02,  ...
          1188.5,  1258.93,  1333.52,  1412.54,  1496.24,   1584.9,  ...
         1678.81,  1778.28,  1883.65,  1995.27,  2113.49,  2238.72,  ...
         2371.38,  2511.89,  2660.73,  2818.39,  2985.39,  3162.28,  ...
         3349.66,  3548.14,  3758.38,  3981.08,  4216.97,  4466.84,  ...
         4731.52,  5011.88,  5308.85,  5623.42,  5956.63,  6309.58,  ...
         6683.45,  7079.47,  7498.95,   7943.3,  8413.97,  8912.52,  ...
         9440.62,    10000,  10592.6,  11220.2,    11885,  12589.3,  ...
         13335.2,  14125.4,  14962.4,    15849,  16788.1,  17782.8,  ...
         18836.5,  19952.7 ...
];

eq.raw_m_db = [ ...
         58.1704,   60.074,  60.4541,  61.3079,  62.8198,  64.3749,  ...
         65.1556,   66.512,  67.5916,  68.6344,  69.9276,  70.7658,  ...
         71.1125,  72.0627,  73.5348,  75.4887,  77.3088,  79.1541,  ...
         80.9627,  82.0607,   81.943,  81.2228,  81.9497,  83.1848,  ...
         84.3375,  85.4993,  86.4642,  87.1179,  87.4424,    87.53,  ...
         85.7425,  85.0095,  82.9405,  82.9242,  82.7327,  83.6423,  ...
         83.2839,  82.4201,  84.1403,  84.6485,  83.9274,  83.3366,  ...
         83.3005,  84.2598,  85.1544,   86.015,  86.6519,  87.2118,  ...
         87.5498,  88.1742,  88.5215,  88.3801,  89.8762,  91.4418,  ...
         93.1845,  94.3355,  95.0918,  95.0258,  94.0337,  91.1068,  ...
         88.7303,  87.4853,  86.2916,   83.037,  79.8056,  78.2022,  ...
         76.0341,  74.5674,  69.2288,  56.1308,   68.697,   69.687,  ...
         68.0005,   64.698 ...
];

%% With parametric IIR EQ try to place peaking EQ at
%  resonant frequencies 1.4 kHz and 7.5 kHz. The gain
%  and Q values were experimented manually. Additionally
%  The lowest and highest frequeciens below and above speaker
%  capability are attenuated with high-pass and low-pass
%  filtering.
if eq.enable_iir;
	eq.peq = [ ...
			 eq.PEQ_HP2   100 NaN NaN; ...
			 eq.PEQ_PN2  1480  -6 2.0; ...
			 eq.PEQ_PN2  7600 -12 1.3; ...
			 eq.PEQ_LP2 14500 NaN NaN; ...
		 ];
end

%% With FIR EQ try to flatten frequency response within
%  1 - 13 kHz frequency band.
if eq.enable_fir
        eq.fir_minph = 1;
	eq.fir_beta = 4;
	eq.fir_length = 63;
	eq.fir_autoband = 0;
	eq.fmin_fir = 1000;
	eq.fmax_fir = 13e3;
end

%% Design EQ
eq = eq_compute(eq);

%% Plot
eq_plot(eq, 1);

%% Export FIR part
platform_max_channels = 2;  % Identical two speakers
assign_response = [0 0];    % Switch to response #0
num_responses = 1;          % Single response
bq_fir = eq_fir_blob_quant(eq.b_fir);
bm_fir = eq_fir_blob_merge(platform_max_channels, ...
		       num_responses, ...
		       assign_response, ...
		       [ bq_fir ]);
bp_fir = eq_fir_blob_pack(bm_fir);
eq_alsactl_write('example_spk_eq_fir.txt', bp_fir);
eq_blob_write('example_spk_eq_fir.blob', bp_fir);

%% Export IIR part
bq_iir = eq_iir_blob_quant(eq.b_p, eq.a_p);
bm_iir = eq_iir_blob_merge(platform_max_channels, ...
		       num_responses, ...
		       assign_response, ...
		       [ bq_iir ]);
bp_iir = eq_iir_blob_pack(bm_iir);
eq_alsactl_write('example_spk_eq_iir.txt', bp_iir);
eq_blob_write('example_spk_eq_iir.blob', bp_iir);
