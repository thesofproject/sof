function y = drc_db2mag(tstruct)
%y = 10.^(ydb/20);     % db2mag = 10.^(21/20)
y = power(tstruct.u1,tstruct.u2/20); % =power(10,(21/20))
end