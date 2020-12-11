function figplot(y)
figure(1)
% complex(y)
real_y = real(y);
imag_y = imag(y);
x = 1:length(y);
loglog(x,real_y,x,imag_y); grid on;
% plot(1:length(y),imag_y,'g-o'); grid on;
legend('real','imag');
title('Fixpt-logarithm-function');xlabel('x-axis');ylabel('y-axis/logVal');
grid on;