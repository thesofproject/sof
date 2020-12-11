function y = drc_round_wrapper_fixpt(x)
    fm = get_fimath();
    x_in = fi( x, 0, 16, 13, fm );
    [y_out] = drc_round_fixpt( x_in );
    y = double( y_out );
end

function fm = get_fimath()
	fm = fimath('RoundingMethod', 'Convergent',...
	     'OverflowAction', 'Wrap',...
	     'ProductMode','FullPrecision',...
	     'SumMode','FullPrecision');
end
