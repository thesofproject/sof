% a_law_mu_law_test_vectors() - Create A-law and mu-law test vectors
%
% Running this script creates header files ref_chirp_mono_8k_s16.h,
% a_law_codec.h, and mu_law_codec.h. The chirp waveform for test
% input is created with sox, and then ended and decoded as
% A-law and mu-law.

% SPDX-License-Identifier: BSD-3-Clause
%
% Copyright(c) 2025 Intel Corporation.

function a_law_mu_law_test_vectors()

  sox = 0;
  alaw = 1;
  mulaw = 1;
  bits = 16;
  ts = 0.5;
  fs = 8e3;
  t = (0:round(ts*fs - 1))/fs;
  x = round(2^(bits - 1) * chirp(t, 100, ts, 3.5e3, 'logarithmic'));
  xmax = 2^(bits - 1) - 1;
  xmin = -2^(bits - 1);
  x = max(min(x, xmax), xmin);
  ref_s16_data = int16(x);

  ref_s16_header_fn = 'ref_chirp_mono_8k_s16.h';
  ref_alaw_header_fn = 'a_law_codec.h';
  ref_mulaw_header_fn = 'mu_law_codec.h';

  close all;
  path(path(), '../../../m');

  if sox
    ref_s16 = '/tmp/chirp_mono_8k_s16.raw';
    ref_alaw_enc = '/tmp/ref_alaw_enc.raw';
    ref_alaw_dec = '/tmp/ref_alaw_dec.raw';
    ref_mulaw_enc = '/tmp/ref_mulaw_enc.raw';
    ref_mulaw_dec = '/tmp/ref_mulaw_dec.raw';
    sox_s16_chirp_gen = sprintf('sox -c 1 -r 8000 -b 16 --encoding signed-integer -n %s synth 0.5 sine 100-3500', ref_s16);
    sox_alaw_enc = sprintf('sox -c 1 -r 8000 -b 16 --encoding signed-integer %s --encoding a-law %s', ref_s16, ref_alaw_enc);
    sox_alaw_dec = sprintf('sox -c 1 -r 8000 --encoding a-law %s -b 16 --encoding signed-integer %s', ref_alaw_enc, ref_alaw_dec);
    sox_mulaw_enc = sprintf('sox -c 1 -r 8000 -b 16 --encoding signed-integer %s --encoding a-law %s', ref_s16, ref_mulaw_enc);
    sox_mulaw_dec = sprintf('sox -c 1 -r 8000 --encoding a-law %s -b 16 --encoding signed-integer %s', ref_mulaw_enc, ref_mulaw_dec);
    system(sox_s16_chirp_gen);
    system(sox_alaw_enc);
    system(sox_alaw_dec);
    system(sox_mulaw_enc);
    system(sox_mulaw_dec);
    ref_s16_data = readbin(ref_s16, 'int16'); figure;
    ref_alaw_enc_data = readbin(ref_alaw_enc, 'uint8');
    ref_alaw_dec_data = readbin(ref_alaw_dec, 'int16');
    ref_mulaw_enc_data = readbin(ref_mulaw_enc, 'uint8');
    ref_mulaw_dec_data = readbin(ref_mulaw_dec, 'int16');
  else
    if alaw
      ref_alaw_enc_data = alaw_enc(ref_s16_data);
      ref_alaw_dec_data = alaw_dec(ref_alaw_enc_data);
    end
    if mulaw
      ref_mulaw_enc_data = mulaw_enc(ref_s16_data);
      ref_mulaw_dec_data = mulaw_dec(ref_mulaw_enc_data);
    end
  end

  if alaw
    plot(ref_s16_data); grid on; title('Input s16');
    figure; plot(ref_alaw_enc_data); grid on; title('A-law data');
    figure; plot(ref_alaw_dec_data); grid on; title('A-law decode s16');
  end

  if mulaw
    figure; plot(ref_mulaw_enc_data); grid on; title('mu-law data');
    figure; plot(ref_mulaw_dec_data); grid on; title('mu-law decode s16');
  end

  fh = export_headerfile_open(ref_s16_header_fn);
  comment = sprintf('Created %s with script a_law_mu_law_test_vectors.m %s', ...
		    datestr(now, 0), export_get_git_describe());
  export_comment(fh, comment);
  fprintf(fh, '#include <stdint.h>\n');
  export_ndefine(fh, 'REF_DATA_SAMPLE_COUNT', length(ref_s16_data));
  export_vector(fh, 16, 'chirp_mono_8k_s16',  ref_s16_data);
  fclose(fh);

  if alaw
    fh = export_headerfile_open(ref_alaw_header_fn);
    comment = sprintf('Created %s with script a_law_mu_law_test_vectors.m %s', ...
		      datestr(now, 0), export_get_git_describe());
    export_comment(fh, comment);
    fprintf(fh, '#include <stdint.h>\n');
    export_vector(fh, 8, 'ref_alaw_enc_data',  ref_alaw_enc_data);
    export_vector(fh, 16, 'ref_alaw_dec_data',  ref_alaw_dec_data);
    fclose(fh);
  end

  if mulaw
    fh = export_headerfile_open(ref_mulaw_header_fn);
    comment = sprintf('Created %s with script a_law_mu_law_test_vectors.m %s', ...
		      datestr(now, 0), export_get_git_describe());
    export_comment(fh, comment);
    fprintf(fh, '#include <stdint.h>\n');
    export_vector(fh, 8, 'ref_mulaw_enc_data',  ref_mulaw_enc_data);
    export_vector(fh, 16, 'ref_mulaw_dec_data',  ref_mulaw_dec_data);
    fclose(fh);
  end

end

function x = readbin(fn, itype)
  fh = fopen(fn, 'r');
  if fh == -1
    fprintf(1, 'Could not open file %s.\n', fn);
    error("Failed.");
  end
  x = fread(fh, inf, itype);
  fclose(fh);
end

% See G.711 alaw compress from
% https://www.itu.int/rec/T-REC-G.191/
function encoded = alaw_enc(input_samples)
  num_samples = length(input_samples);
  in16 = int16(input_samples);
  encoded_samples = uint8(zeros(num_samples, 1));
  for n = 1:num_samples
    if in16(n) < 0
      ix = bitshift(-in16(n) -1, -4); % 1's complement
    else
      ix = bitshift(in16(n), -4);
    end

    if ix > 15
      iexp = 1;
      while (ix > 16 + 15)
	ix = bitshift(ix, -1);
	iexp = iexp + 1;
      end

      ix = ix - 16;
      ix = ix + bitshift(iexp, 4);
    end

    if in16(n) >= 0
      ix = bitor(ix, 128);
    end

    encoded(n) = bitxor(ix, 85);
  end
end

% See G.711 alaw expand from
% https://www.itu.int/rec/T-REC-G.191/
function samples_s16 = alaw_dec(input_bytes)
  num_samples = length(input_bytes);
  samples_s16 = int16(zeros(num_samples, 1));
  for n = 1:num_samples
    ix = bitxor(int16(input_bytes(n)), 85);
    ix = bitand(ix, 127);
    iexp = bitshift(ix, -4);
    mant = bitand(ix, 15);
    if iexp > 0
      mant = mant + 16;
    end

    mant = bitshift(mant, 4) + 8;
    if iexp > 1
      mant = bitshift(mant, iexp - 1);
    end

    if input_bytes(n) > 127
      samples_s16(n) = mant;
    else
      samples_s16(n) = -mant;
    end
  end
end

% See G.711 ulaw compress from
% https://www.itu.int/rec/T-REC-G.191/
function encoded = mulaw_enc(input_samples)
  num_samples = length(input_samples);
  in16 = int16(input_samples);
  encoded_samples = uint8(zeros(num_samples, 1));
  for n = 1:num_samples
    if in16(n) < 0
      absno = bitshift(-in16(n) -1, -2) + 33; % 1's complement
    else
      absno = bitshift(in16(n), -2) + 33;
    end

    absno = min(absno, 8191);
    i = bitshift(absno, -6);
    segno = 1;
    while i > 0
      segno = segno + 1;
      i = bitshift(i, -1);
    end

    high_nibble = 8 - segno;
    low_nibble = bitand(bitshift(absno, -segno), 15);
    low_nibble = 15 - low_nibble;
    encoded(n) = bitor(bitshift(high_nibble, 4), low_nibble);
    if  in16(n) >= 0
      encoded(n) = bitor(encoded(n), 128);
    end
  end
end

% See G.711 alaw expand from
% https://www.itu.int/rec/T-REC-G.191/
function samples_s16 = mulaw_dec(input_bytes)
  num_samples = length(input_bytes);
  samples_s16 = int16(zeros(num_samples, 1));
  for n = 1:num_samples
    if input_bytes(n) < 128
      sign = -1;
    else
      sign = 1;
    end

    mantissa = -int16(input_bytes(n)) - 1; % 1's complement
    exponent = bitand(bitshift(mantissa, -4), 7);
    segment = exponent + 1;
    mantissa = bitand(mantissa, 15);
    step = bitshift(4, segment);
    samples_s16(n) = sign * (bitshift(128, exponent) + step * mantissa + step / 2 - 4 * 33);
  end
end
