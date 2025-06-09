% Compute MEL every 1s
tc = 1.0;

% Load sound clip
sound_clip = "/usr/share/sounds/alsa/Front_Right.wav";
[x1, fs] = audioread(sound_clip);
if size(x1, 2) == 1
	x1 = repmat(x1, 1, 2);
end
sx = size(x1);
num_frames = sx(1);
num_channels = sx(2);

% load A-weight filters
load sof_sound_dose_time_domain_filters.mat

% Filter with IIR, FIR
x2 = filter(b_iir, a_iir, x1);
x3 = filter(b_fir, 1, x2);

%figure
%plot(x1)

%figure
%plot(x3)

% compute RMS level in 1s chunks
np = 1;
nc = tc * fs;
num_chunks = floor(num_frames/nc);
l_dbfs_all = zeros(num_chunks, num_channels);
mel_all = zeros(num_chunks, 1);

dbfs_offs = 3.01;
dbfs_offs_weight_filters = 3;
i1 = 1;
for n = 1:num_chunks
  sum = 0;
  i2 = i1 + nc - 1;
  for ch = 1:num_channels
    y = x3(i1:i2, ch);
    sum = sum + mean(y.^2);
    l_dbfs = 20*log10(sqrt(mean(y.^2))) + dbfs_offs + dbfs_offs_weight_filters;
    l_dbfs_all(np, ch) = l_dbfs;
  end
  mel = 10*log10(sum) + dbfs_offs_weight_filters;
  if num_channels > 1
    mel = mel - 1.5 * num_channels;
  end
  mel_all(np) = mel;
  i1 = i1 + nc;
  np = np + 1;
end

figure(1)
plot(l_dbfs_all)
grid on
xlabel('Time (s)');
ylabel('Level (dBSFS)');

figure(2)
plot(mel_all)
grid on
xlabel('Time (s)');
ylabel('MEL (dB)');

mel_all

mel_all_rnd = round(mel_all)
