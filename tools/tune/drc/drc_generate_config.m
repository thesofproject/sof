function config = drc_generate_config(coefs);

addpath ./../eq

config.enabled = coefs.enabled;
config.db_threshold = eq_coef_quant(coefs.db_threshold, 32, 24); % Q8.24
config.db_knee = eq_coef_quant(coefs.db_knee, 32, 24); % Q8.24
config.ratio = eq_coef_quant(coefs.ratio, 32, 24); % Q8.24
config.pre_delay_time = eq_coef_quant(coefs.pre_delay_time, 32, 30); % Q2.30
config.linear_threshold = eq_coef_quant(coefs.linear_threshold, 32, 30); % Q2.30
config.slope = eq_coef_quant(coefs.slope, 32, 30); % Q2.30
config.K = eq_coef_quant(coefs.K, 32, 20); % Q12.20
config.knee_alpha = eq_coef_quant(coefs.knee_alpha, 32, 24); % Q8.24
config.knee_beta = eq_coef_quant(coefs.knee_beta, 32, 24); % Q8.24
config.knee_threshold = eq_coef_quant(coefs.knee_threshold, 32, 24); % Q8.24
config.ratio_base = eq_coef_quant(coefs.ratio_base, 32, 30); % Q2.30
config.master_linear_gain = eq_coef_quant(coefs.master_linear_gain, 32, 24); % Q8.24
config.one_over_attack_frames = eq_coef_quant(coefs.one_over_attack_frames, 32, 30); % Q2.30
config.sat_release_frames_inv_neg = eq_coef_quant(coefs.sat_release_frames_inv_neg, 32, 30); % Q2.30
config.sat_release_rate_at_neg_two_db = eq_coef_quant(coefs.sat_release_rate_at_neg_two_db, 32, 30); % Q2.30
config.kSpacingDb = coefs.kSpacingDb;
config.kA = eq_coef_quant(coefs.kA, 32, 12); % Q20.12
config.kB = eq_coef_quant(coefs.kB, 32, 12); % Q20.12
config.kC = eq_coef_quant(coefs.kC, 32, 12); % Q20.12
config.kD = eq_coef_quant(coefs.kD, 32, 12); % Q20.12
config.kE = eq_coef_quant(coefs.kE, 32, 12); % Q20.12

% Print out config
config

rmpath ./../eq
end
