function [tstruct] = init_struc ()
tstruct.u1 = (10);
tstruct.u2 = (21);  % raise pow should be positive
% tstruct.u2(find(tstruct.u2>=2^7)) = -2^8 + tstruct.u2(find(tstruct.u2>=2^7));
% size(tstruct.u1)
% size(tstruct.u2)