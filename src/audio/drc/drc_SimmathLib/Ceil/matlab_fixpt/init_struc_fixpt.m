function x = init_struc_fixpt ()
fm = get_fimath();

x = fi(6.95454545454, 0, 32, 29, fm)
end


function fm = get_fimath()
	fm = fimath('RoundingMethod', 'Convergent',...
	     'OverflowAction', 'Wrap',...
	     'ProductMode','FullPrecision',...
	     'SumMode','FullPrecision');
end
