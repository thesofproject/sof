function dcblock_plot_transferfn(R, fs)
% Plot the transfer function.
% For a DC Blocking filter: H(z) = (1-1/z)/(1 - R/z)
% Therefore the coefficients are b = [1 -1], a = [1 -R]
b = [1 -1];
a = [1 -R];

f = linspace(1, fs/2, 500);

semilogx(f, 20*log10(abs(freqz(b, a, f, fs))));
grid on
xlabel('Frequency (Hz)');
ylabel('Magnitude (dB)');
tstr = sprintf("DC Blocking Filter Frequency Response, R = %i", R);
title(tstr);

end
