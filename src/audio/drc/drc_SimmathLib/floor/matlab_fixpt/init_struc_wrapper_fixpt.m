
function x = init_struc_wrapper_fixpt
    fm = get_fimath();
    [x_out] = init_struc_fixpt();
    x = double( x_out );
end

function fm = get_fimath()
	fm = fimath('RoundingMethod', 'Convergent',...
	     'OverflowAction', 'Wrap',...
	     'ProductMode','FullPrecision',...
	     'SumMode','FullPrecision');
end
