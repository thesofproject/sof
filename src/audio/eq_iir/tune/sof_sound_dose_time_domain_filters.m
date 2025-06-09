% Export time domain IIR and FIR filter to apply A-weight for sound dose.
%
% Usage:
% sof_sound_dose_time_domain_filters()

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2025, Intel Corporation. All rights reserved.

function sof_sound_dose_time_domain_filters()

  sof_sound_dose = '../../sound_dose';
  sof_ctl = '../../../../tools/ctl';
  fir_coef_fn = fullfile(sof_sound_dose, 'sound_dose_fir_48k.h');
  iir_coef_fn = fullfile(sof_sound_dose, 'sound_dose_iir_48k.h');
  fir_blob_fn = fullfile(sof_ctl, 'ipc4/eq_fir/sound_dose_fir_48k.txt');
  iir_blob_fn = fullfile(sof_ctl, 'ipc4/eq_iir/sound_dose_iir_48k.txt');
  fir_blob_vn = 'sound_dose_fir_48k';
  iir_blob_vn = 'sound_dose_iir_48k';
  ipc_ver = 4;

  sof_eq_paths(true);
  eq = sound_dose_filters(48e3);

  % Export FIR
  bq = sof_eq_fir_blob_quant(eq.b_fir);
  channels_in_config = 2;  % Setup max 2 channels EQ
  assign_response = [0 0]; % Same response for L and R
  num_responses = 1;       % One response
  bm = sof_eq_fir_blob_merge(channels_in_config, ...
			     num_responses, ...
			     assign_response, ...
			     bq);
  bp = sof_eq_fir_blob_pack(bm, ipc_ver);
  sof_export_c_eq_uint32t(fir_coef_fn, bp, fir_blob_vn, 0);
  sof_alsactl_write(fir_blob_fn, bp);

  % Export IIR
  coefq = sof_eq_iir_blob_quant(eq.p_z, eq.p_p, eq.p_k);
  channels_in_config = 2;  % Setup max 2 channels EQ
  assign_response = [0 0]; % Same response for L and R
  num_responses = 1;       % One response
  coefm = sof_eq_iir_blob_merge(channels_in_config, ...
		                num_responses, ...
		                assign_response, ...
		                coefq);
  coefp = sof_eq_iir_blob_pack(coefm, ipc_ver);
  sof_export_c_eq_uint32t(iir_coef_fn, coefp, iir_blob_vn, 0);
  sof_alsactl_write(iir_blob_fn, coefp);
  sof_eq_paths(false);

  % Export mat file
  b_fir = eq.b_fir;
  [b_iir, a_iir] = zp2tf( eq.p_z, eq.p_p, eq.p_k);
  save sof_sound_dose_time_domain_filters.mat b_fir b_iir a_iir;

end

function eq = sound_dose_filters(fs)


  np = 200;
  f_hi = 0.95 * fs/2;
  fv = logspace(log10(10), log10(f_hi), np);
  ref_a_weight = func_a_weight_db(fv);

  eq = sof_eq_defaults();
  eq.fs = fs;
  eq.enable_fir = 1;
  eq.enable_iir = 1;
  eq.iir_norm_type = '1k'; % At 1 kHz -3 dB
  eq.iir_norm_offs_db = -3;
  eq.fir_norm_type = '1k'; % At 1 kHz 0 dB, total -3 dB gain to avoid clip
  eq.fir_norm_offs_db =  0;
  eq.p_fmin = 10;
  eq.p_fmax = 30e3;

  eq.fir_minph = 1;
  eq.fir_beta = 4;
  eq.fir_length = 31;
  eq.fir_autoband = 0;
  eq.fmin_fir = 300;
  eq.fmax_fir = f_hi;

  eq.raw_f = fv;
  eq.raw_m_db = zeros(1, length(fv));
  eq.target_f = fv;
  eq.target_m_db = ref_a_weight;

  eq.peq = [ ...
	   eq.PEQ_HP2   25 NaN NaN; ...
	   eq.PEQ_HP2   280 NaN NaN; ...
	   eq.PEQ_PN2   245 -5.8 0.5; ...
	 ];

  eq = sof_eq_compute(eq);
  sof_eq_plot(eq, 1);
  figure(3);
  axis([10 20e3 -60 10]);


end

% See https://en.wikipedia.org/wiki/A-weighting
% IEC 61672-1:2013 Electroacoustics - Sound level meters,
% Part 1: Specifications. IEC. 2013.

function a_weight = func_a_weight_db(fv)
  a_weight = 20*log10(func_ra(fv)) - 20*log10(func_ra(1000));
end

function y = func_ra(f)
  f2 = f.^2;
  f4 = f.^4;
  c1 = 12194^2;
  c2 = 20.6^2;
  c3 = 107.7^2;
  c4 = 737.9^2;
  c5 = 12194^2;
  y = c1 * f4 ./ ((f2 + c2).*sqrt((f2 + c3).*(f2 + c4).*(f2 + c5)));
end
