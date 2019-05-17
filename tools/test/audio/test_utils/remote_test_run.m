%% Run test with remote/local playback and capture
%
%  remote_test_run(test);
%
%  Inputs:
%  test.play_cfg - configuration for playback device
%  test.fn_in    - filename for audio input
%  test.length_t - length of audio input in seconds
%  test.rec_cfg  - configuration for audio capture device
%  test.fn_out   - filename for audio output
%
%  Outputs:
%

% SPDX-License-Identifier: BSD-3-Clause
% Copyright(c) 2019 Intel Corporation. All rights reserved.
% Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

function test = remote_test_run(test)

remote_copy_playback(test.fn_in, test.play_cfg);
tcap = floor(3 + test.length_t);
remote_capture(test.fn_out, test.rec_cfg, tcap);
remote_play(test.fn_in, test.play_cfg);
pause(3);
remote_copy_capture(test.fn_out, test.rec_cfg);

end

function remote_copy_playback(fn, cfg)
if cfg.ssh
	cmd = sprintf('scp %s %s:%s/', fn, cfg.user, cfg.dir);
	fprintf('Remote copy: %s\n', cmd);
	system(cmd);
else
	cmd = sprintf('cp %s %s/', fn, cfg.dir);
	fprintf('Local copy: %s\n', cmd);
end
end

function y = remote_copy_capture(fn, cfg)
if cfg.ssh
	cmd = sprintf('scp %s:%s/%s %s', cfg.user, cfg.dir, fn, fn);
	del = sprintf('ssh %s rm %s/%s', cfg.user, cfg.dir, fn);
	fprintf('Remote copy: %s\n', cmd);
else
	cmd = sprintf('cp %s/%s %s', cfg.dir, fn, fn);
	del = sprintf('rm %s/%s', cfg.dir, fn);
	fprintf('Local copy: %s\n', cmd);
end
system(cmd);
system(del);
end

function remote_play(fn, cfg)
if cfg.ssh
	cmd = sprintf('ssh %s aplay -D%s %s/%s', cfg.user, cfg.dev, cfg.dir, fn);
	del = sprintf('ssh %s rm %s/%s', cfg.user, cfg.dir, fn);
	fprintf('Remote play: %s\n', cmd);
else
	cmd = sprintf('aplay -D%s %s/%s', cfg.dev, cfg.dir, fn);
	del = sprintf('rm %s/%s', cfg.dir, fn);
	fprintf('Local play: %s\n', cmd);
end
system(cmd);
system(del);
end

function remote_capture(fn, cfg, t)
if cfg.ssh
	cmd = sprintf('ssh %s arecord -q -D%s %s -d %d %s/%s &', ...
		cfg.user, cfg.dev, cfg.fmt, t, cfg.dir, fn);
	fprintf('Remote capture: %s\n', cmd);
else
	cmd = sprintf('arecord -q -D%s %s -d %d %s/%s &', ...
		cfg.dev, cfg.fmt, t, cfg.dir, fn);
	fprintf('Local capture: %s\n', cmd);
end
system(cmd);
end
