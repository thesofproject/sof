
function y = drc_floor_wrapper_fixpt(x)
    fm = get_fimath();
    x_in = fi( x, 0, 32, 31, fm );
    [y_out] = drc_floor_fixpt( x_in );
    y = double( y_out );
end

function fm = get_fimath()
	fm = fimath('RoundingMethod', 'Convergent',...
	     'OverflowAction', 'Wrap',...
	     'ProductMode','FullPrecision',...
	     'SumMode','FullPrecision');
end
