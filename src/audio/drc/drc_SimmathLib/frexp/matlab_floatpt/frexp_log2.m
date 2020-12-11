% Author : Sriram Shastry
% Company: Intel Co-orporation
% [F,E] = log2(X) corresponds to the ANSIÂ® C function frexp()
%  IEEE standard function logb().
% [F,E] = log2(X) for each element of the real array X, returns an
%         array F of real numbers, usually in the range 0.5 <= abs(F) < 1,
%         and an array E of integers, so that X = F .* 2.^E.
% Any zeros in X produce F = 0 and E = 0.
% function [F,E ] = frexp_log2(x)
% x: real vector or matrix
% f: array of real values, usually in the range 0.5<= abs(f)<1.
% e: array of integers that satisfy the equation: x = f.*2.ˆe
% function [yVal ] = frexp_log2(x)
function [F,E ] = frexp_log2(x)
[F,E] = log2(double(x));

% [yVal]  = log(x)/log(2); % log2(x) yval= log(val)/log(2)
end