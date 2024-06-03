function dcblock_plot_stepfn(R, fs)
% Plot the step response of a DC Blocking Filter
% For a DC Blocking filter: H(z) = (1-1/z)/(1 - R/z)
% Therefore the coefficients are b = [1 -1], a = [1 -R]
b = [1 -1];
a = [1 -R];

t=((1:0.5*fs)-1)/fs;
plot(t, filter(b, a, ones(length(t), 1)))
grid on
xlabel('Samples');
ylabel('Amplitude');
tstr = sprintf("DC Blocking Filter Step Response, R = %i", R);
title(tstr);

end
