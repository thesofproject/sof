function figplot(ydB)
figure(1)
real_z1 = real(ydB);
imag_z1 = imag(ydB);

plot(1:length(ydB),real_z1,'b-x'); grid on; hold on;
plot(1:length(ydB),imag_z1,'g-x'); grid on; hold on;
title('Fixpt-logarithm');xlabel('Num');ylabel('values[log]');
legend('real','imaginary');
grid on;