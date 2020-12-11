function figplot(y)
figure(1)
plot(1:length(y),y,'b-x'); grid on;
title('Fixpt-pow');xlabel('Num');ylabel('values[pow]');
grid on;
end