function bf = bf_defaults()

% Recording array general setup
bf.fs = 16e3;        % Design for 16 kHz sample rate
bf.c = 343;          % Speed of sound in 20C
bf.steer_az = 0;     % Azimuth 0 deg
bf.steer_el = 0;     % Elevation 0 deg
bf.steer_r = 5.0;    % Distance 5.0m
bf.fir_length = 64;  % 64 tap FIR filters
bf.fir_beta = 10;    % Beta for kaiser window method FIR design
bf.mu_db = -50;      % dB of diagonal loading to noise covariance matrix
bf.do_plots = 1;
bf.plot_box = 0.20;  % Show 20cm wide plot cube for array geometry
bf.array_id = '';
bf.array_angle = [0 0 0]; % Array rotation angles for xyz
bf.tplg_fn = '';
bf.sofctl_fn = '';
bf.mat_fn = '';
bf.tplg_path = '../../topology/m4/tdfb';
bf.sofctl_path = '../../ctl/tdfb';
bf.data_path = './data';
bf.endian = 'little';
bf.fn = 1;
bf.sinerot_a = 10^(-20/20);
bf.sinerot_f = 2e3;
bf.sinerot_t = 1.0;
bf.sinerot_az_step = 5;
bf.sinerot_az_start = -180;
bf.sinerot_az_stop = 180;
bf.sinerot_fn = '';
bf.diffuse_fn = '';
bf.diffuse_t = 1;
bf.diffuse_lev = -20;
bf.random_fn = '';
bf.random_t = 1;
bf.random_lev = -20;
bf.num_output_channels = 1;
bf.num_output_streams = 1;
bf.input_channel_select = [];
bf.output_channel_mix = [];
bf.output_stream_mix = [];
bf.num_filters = [];

end
