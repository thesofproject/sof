function y = drc_db2mag_wrapper_fixpt(tstruct)
    fm = get_fimath();
    tstruct_in = copyTo_tstruct_in( tstruct );
    [y_out] = drc_db2mag_fixpt( tstruct_in );
    y = double( y_out );
end
function tstruct_in = copyTo_tstruct_in(tstruct)
    coder.inline( 'always' );
    fm = get_fimath();
    tstruct_in.u1 = fi( tstruct.u1, 0, 4, 0, fm );
    tstruct_in.u2 = fi( tstruct.u2, 0, 5, 0, fm );
end

function fm = get_fimath()
	fm = fimath('RoundingMethod', 'Convergent',...
	     'OverflowAction', 'Wrap',...
	     'ProductMode','FullPrecision',...
	     'SumMode','FullPrecision');
end
