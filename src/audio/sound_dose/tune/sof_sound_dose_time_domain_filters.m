% Export time domain IIR and FIR filter to apply A-weight for sound dose.
%
% Usage:
% sof_sound_dose_time_domain_filters()

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright (c) 2025, Intel Corporation.

function sof_sound_dose_time_domain_filters(fs)

  if exist('OCTAVE_VERSION', 'builtin')
    pkg load signal;
  end

  sof_sound_dose_time_domain_filters_for_rate(48e3);
  sof_sound_dose_time_domain_filters_for_rate(44.1e3);

end

function sof_sound_dose_time_domain_filters_for_rate(fs)

  sof_sound_dose = '../../sound_dose';
  sof_ctl = '../../../../tools/ctl';
  sound_dose_paths(true);


  fs_str = sprintf('%dk', round(fs/1000));
  fir_str = sprintf('sound_dose_fir_%s', fs_str);
  iir_str = sprintf('sound_dose_iir_%s', fs_str);
  prm.fir_coef_fn = fullfile(sof_sound_dose, [fir_str '.h']);
  prm.iir_coef_fn = fullfile(sof_sound_dose, [iir_str '.h']);
  prm.fir_blob_fn = fullfile(sof_ctl, ['ipc4/eq_fir/' fir_str '.txt']);
  prm.iir_blob_fn = fullfile(sof_ctl, ['ipc4/eq_iir/' iir_str '.txt']);
  prm.fir_blob_vn = fir_str;
  prm.iir_blob_vn = iir_str;
  prm.ipc_ver = 4;
  eq = sound_dose_filters(fs);
  export_filters(eq, prm);

  sound_dose_paths(false);
end

function export_filters(eq, prm)

  b_fir = 1;
  b_iir = 1;
  a_iir = 1;
  howto = 'cd src/audio/sound_dose/tune; octave --quiet --no-window-system sof_sound_dose_time_domain_filters.m';

  % Export FIR
  if eq.enable_fir
    bq = sof_eq_fir_blob_quant(eq.b_fir);
    channels_in_config = 2;  % Setup max 2 channels EQ
    assign_response = [0 0]; % Same response for L and R
    num_responses = 1;       % One response
    bm = sof_eq_fir_blob_merge(channels_in_config, ...
			       num_responses, ...
			       assign_response, ...
			       bq);
    bp = sof_eq_fir_blob_pack(bm, prm.ipc_ver);
    sof_export_c_eq_uint32t(prm.fir_coef_fn, bp, prm.fir_blob_vn, 0, howto);
    sof_alsactl_write(prm.fir_blob_fn, bp);
    b_fir = eq.b_fir;
  end

  % Export IIR
  if eq.enable_iir
    coefq = sof_eq_iir_blob_quant(eq.p_z, eq.p_p, eq.p_k);
    channels_in_config = 2;  % Setup max 2 channels EQ
    assign_response = [0 0]; % Same response for L and R
    num_responses = 1;       % One response
    coefm = sof_eq_iir_blob_merge(channels_in_config, ...
		                  num_responses, ...
		                  assign_response, ...
		                  coefq);
    coefp = sof_eq_iir_blob_pack(coefm, prm.ipc_ver);
    sof_export_c_eq_uint32t(prm.iir_coef_fn, coefp, prm.iir_blob_vn, 0, howto);
    sof_alsactl_write(prm.iir_blob_fn, coefp);
    [b_iir, a_iir] = zp2tf( eq.p_z, eq.p_p, eq.p_k);
  end

  % Export mat file
  save sof_sound_dose_time_domain_filters.mat b_fir b_iir a_iir;

end

function eq = sound_dose_filters(fs)

  np = 200;
  f_hi = 0.95 * fs/2;
  fv = logspace(log10(10), log10(f_hi), np);
  ref_a_weight = func_a_weight_db(fv);

  eq = sof_eq_defaults();
  eq.fs = fs;
  eq.enable_fir = 0;
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
	   eq.PEQ_HP1   12    0   0; ...
	   eq.PEQ_HP1   20    0   0; ...
	   eq.PEQ_HP2   280   0   0; ...
	   eq.PEQ_PN2   245  -5.45 0.5; ...
	   eq.PEQ_HS1  13000 -3   0; ...
	   eq.PEQ_HS1  14000 -3   0; ...
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

function sound_dose_paths(enable)
  switch enable
    case true
      addpath('../../eq_iir/tune');
      addpath('../../../../tools/tune/common');
    case false
      rmpath('../../eq_iir/tune');
      rmpath('../../../../tools/tune/common');
  end
end
