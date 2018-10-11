function test = chirp_test_analyze(test)

%%
% Copyright (c) 2017, Intel Corporation
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

%% Load output file
[x, nx] = load_test_output(test);
if nx < 1
	test.fail = -1; % No data
	return;
end

%% Find sync
[d, nt, nt_use, nt_skip] = find_test_signal(x(:,test.ch(1)), test);

%% Trim sample
i1 = d+nt_skip;
i2 = i1+nt_use-1;
z = x(i1:i2, :);
y = z(:,test.ch);
s = sum(z,2);

%% Quick check length, RMS, offset, and channels sum
sz = size(z);
tz = sz(1)/test.fs;
et = abs(test.cl-tz)/test.cl;
rms_db = 10*log10(mean(z.^2)) + 20*log10(sqrt(2));
offs_db = 20*log10(abs(mean(z)));
sum_max_db = 20*log10(max(abs(s)));
% Check for proper ratio of out/in samples, minimum level, maximum offset
% and maximum of sum of channels. The input is such that the channels should
% sum to zero. A phase difference in channels would cause non-zero output
% for channels sum. Dithered input causes a small non-zero sum value.

if et > 0.05
	fail = 1;
	fprintf('Failed output chirp length, err=%f, t=%f.\n', et, tz);
else if (min(rms_db) < -3) && (test.fs2 + 1 > test.fs1)
	     fail = 1;
	     fprintf('Failed output chirp level.\n');
     else if max(offs_db) > -40
		  fail = 1;
		  fprintf('Failed output chirp DC offset.\n');
	  else if (sum_max_db > -100) && (mod(test.nch, 2) == 0)
		       fail = 1;
		       fprintf('Failed output chirp channels phase.\n');
	       else
		       fail = 0;
	       end
	  end
     end
end

test.fh = figure('visible', test.visible);
ns = 1024;
no = round(0.9*ns);
specgram(y(:,1), ns, test.fs, kaiser(ns,27), no);
colormap('jet');
caxis([-150 0]);
colorbar('EastOutside');

test.fail = fail;

end
