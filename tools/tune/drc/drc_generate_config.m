function config = drc_generate_config(enabled);

% Temporarily use a fixed set of parameters
db_threshold = -32.000000
db_knee = 23.000000
ratio = 12.000000
pre_delay_time = 0.006000
linear_threshold = 0.025119
slope = 0.083333
K = 10.974278
knee_alpha = 0.116241
knee_beta = -0.120044
knee_threshold = 0.354813
ratio_base = 0.124059
master_linear_gain = 3.791611
one_over_attack_frames = 0.00113378684
sat_release_frames_inv_neg = -0.009070
sat_release_rate_at_neg_two_db = 0.002091
kSpacingDb = 5
kA = 793.800049
kB = 52.820679
kC = 444.612061
kD = 111.620804
kE = 8.346558

addpath ./../eq

config.enabled = enabled
config.db_threshold = eq_coef_quant(db_threshold, 32, 24) % Q8.24
config.db_knee = eq_coef_quant(db_knee, 32, 24) % Q8.24
config.ratio = eq_coef_quant(ratio, 32, 24) % Q8.24
config.pre_delay_time = eq_coef_quant(pre_delay_time, 32, 30) % Q2.30
config.linear_threshold = eq_coef_quant(linear_threshold, 32, 30) % Q2.30
config.slope = eq_coef_quant(slope, 32, 30) % Q2.30
config.K = eq_coef_quant(K, 32, 20) % Q12.20
config.knee_alpha = eq_coef_quant(knee_alpha, 32, 24) % Q8.24
config.knee_beta = eq_coef_quant(knee_beta, 32, 24) % Q8.24
config.knee_threshold = eq_coef_quant(knee_threshold, 32, 24) % Q8.24
config.ratio_base = eq_coef_quant(ratio_base, 32, 30) % Q2.30
config.master_linear_gain = eq_coef_quant(master_linear_gain, 32, 24) % Q8.24
config.one_over_attack_frames = eq_coef_quant(one_over_attack_frames, 32, 30) % Q2.30
config.sat_release_frames_inv_neg = eq_coef_quant(sat_release_frames_inv_neg, 32, 30) % Q2.30
config.sat_release_rate_at_neg_two_db = eq_coef_quant(sat_release_rate_at_neg_two_db, 32, 30) % Q2.30
config.kSpacingDb = kSpacingDb
config.kA = eq_coef_quant(kA, 32, 12) % Q20.12
config.kB = eq_coef_quant(kB, 32, 12) % Q20.12
config.kC = eq_coef_quant(kC, 32, 12) % Q20.12
config.kD = eq_coef_quant(kD, 32, 12) % Q20.12
config.kE = eq_coef_quant(kE, 32, 12) % Q20.12

config.num_coeffs = 22 % Number of coefficients above

rmpath ./../eq
end
