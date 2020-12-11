function tstruct = init_struc_wrapper_fixpt
    fm = get_fimath();
    [tstruct_out] = init_struc_fixpt();
    tstruct = copyTo_tstruct( tstruct_out );
end
function tstruct = copyTo_tstruct(tstruct_out)
    coder.inline( 'always' );
    fm = get_fimath();
    tstruct.u1 = double( tstruct_out.u1 );
    tstruct.u2 = double( tstruct_out.u2 );
end

function fm = get_fimath()
	fm = fimath('RoundingMethod', 'Convergent',...
	     'OverflowAction', 'Wrap',...
	     'ProductMode','FullPrecision',...
	     'SumMode','FullPrecision');
end
