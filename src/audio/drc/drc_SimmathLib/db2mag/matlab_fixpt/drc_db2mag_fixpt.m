function y = drc_db2mag_fixpt(tstruct)
%y = 10.^(ydb/20);     % db2mag = 10.^(21/20)
fm = get_fimath();

y = fi(power(fi_toint(tstruct.u1),fi_toint(tstruct.u2*fi(1/20, 0, 32, 36, fm))), 0, 32, 28, fm); % =power(10,(21/20))
end



function y = fi_toint(u)
    coder.inline( 'always' );
    if isfi( u )
        nt = numerictype( u );
        s = nt.SignednessBool;
        wl = nt.WordLength;
        y = int32( fi( u, s, wl, 0, hdlfimath ) );
    else
        y = int32( u );
    end
end

function fm = get_fimath()
	fm = fimath('RoundingMethod', 'Convergent',...
	     'OverflowAction', 'Wrap',...
	     'ProductMode','FullPrecision',...
	     'SumMode','FullPrecision');
end
