function [tstruct] = init_struc_fixpt ()
fm = get_fimath();

tstruct.u1 = fi((10), 0, 4, 0, fm);
tstruct.u2 = fi((21), 0, 5, 0, fm);  % raise pow should be positive
% tstruct.u2(find(tstruct.u2>=2^7)) = -2^8 + tstruct.u2(find(tstruct.u2>=2^7));
% size(tstruct.u1)
% size(tstruct.u2)
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
