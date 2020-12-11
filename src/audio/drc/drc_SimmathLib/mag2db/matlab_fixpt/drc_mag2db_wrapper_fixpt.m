function ydB = drc_mag2db_wrapper_fixpt(tstruct)
    fm = get_fimath();
    tstruct_in = copyTo_tstruct_in( tstruct );
    [ydB_out] = drc_mag2db_fixpt( tstruct_in );
    ydB = ydB_out;
end
function tstruct_in = copyTo_tstruct_in(tstruct)
    coder.inline( 'always' );
    fm = get_fimath();
    tstruct_in.x = fi( tstruct.x, 0, 4, 0, fm );
    tstruct_in.ydB = fi( tstruct.ydB, 0, 1, 0, fm );
end

function fm = get_fimath()
	fm = fimath('RoundingMethod', 'Convergent',...
	     'OverflowAction', 'Wrap',...
	     'ProductMode','FullPrecision',...
	     'SumMode','FullPrecision');
end
