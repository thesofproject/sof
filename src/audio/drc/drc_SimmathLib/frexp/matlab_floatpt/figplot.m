function  figplot(F,E)
x =1;
plot(x,F,'r-o',x,E,'g-x'); grid on;
legend('F-RealNum','E-integer');
xlabel('X-axis');ylabel('Y-axis');
title('Base 2 logarithm and dissect floating point number');

% function  figplot(yval)
% x =1;
% plot(x,yval,'r-o'); grid on;
% legend('frexp[mag]');
% xlabel('X-axis');ylabel('Y-axis');
% title('Base 2 logarithm and dissect floating point number');
