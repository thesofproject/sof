function y = drc_asin_wrapper_fixpt(x)
    fm = get_fimath();
    x_in = fi( x, 1, 2, 0, fm );
    [y_out] = drc_asin_fixpt( x_in );
    y = double( y_out );
end

function fm = get_fimath()
	fm = fimath('RoundingMethod', 'Convergent',...
	     'OverflowAction', 'Wrap',...
	     'ProductMode','FullPrecision',...
	     'SumMode','FullPrecision');
end
