# Export FFT twiddle factors data

To generate the twiddle factors headers for FFT max size of 1024
(current assumption) run these shell commands:

octave -q --eval "sof_export_twiddle(32, 'twiddle_32.h', 1024);"
octave -q --eval "sof_export_twiddle(16, 'twiddle_16.h', 1024);"
cp twiddle_32.h ../../../include/sof/audio/coefficients/fft/
cp twiddle_16.h ../../../include/sof/audio/coefficients/fft/

To generate the twiddle factors for the non-power-of-two FFT implementation for max
size 3072 run these shell commands:

octave -q --eval "sof_export_twiddle(32, 'twiddle_3072_32.h', 2048, 3072, 'FFT_MULTI_TWIDDLE_SIZE', 'multi_twiddle');"
cp twiddle_3072_32.h ../../../include/sof/audio/coefficients/fft/
