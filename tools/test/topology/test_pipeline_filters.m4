
# Define the algorithm configurations blobs to apply such as filter coefficients

# EQs
ifelse(TEST_PIPE_NAME, `eq-iir', `define(PIPELINE_FILTER1, `eq_iir_coef_loudness.m4')')
ifelse(TEST_PIPE_NAME, `eq-fir', `define(PIPELINE_FILTER2, `eq_fir_coef_mid.m4')')

# TDFB test pipelines with different names for different configurations

# line array 2 mic
ifelse(TEST_PIPE_NAME, `tdfb', `define(PIPELINE_FILTER1, `tdfb/coef_line2_50mm_pm90deg_48khz.m4')')

# line array 4 mic
ifelse(TEST_PIPE_NAME, `tdfb_line4_28mm_pm90deg_48khz',
`
define(PIPELINE_FILTER1, `tdfb/coef_line4_28mm_pm90deg_48khz.m4')
define(TEST_PIPE_NAME, `tdfb')
')

# circular 8 mic, use 360 degree enum controls variant while line array
# is 180 degrees
ifelse(TEST_PIPE_NAME, `tdfb_circular8_100mm_az0el0deg_48khz',
`
define(PIPELINE_FILTER1, `tdfb/coef_circular8_100mm_az0el0deg_48khz.m4')
define(TEST_PIPE_NAME, `tdfb360')
')

# circular 8 mic, use 360 degree enum controls variant while line array
# is 180 degrees
ifelse(TEST_PIPE_NAME, `tdfb_circular8_100mm_pm30deg_48khz',
`
define(PIPELINE_FILTER1, `tdfb/coef_circular8_100mm_pm30deg_48khz.m4')
define(TEST_PIPE_NAME, `tdfb360')
')
