function x = init_struc_fixpt ()
fm = get_fimath();

x = fi([-1,1], 1, 2, 0, fm);
end


function fm = get_fimath()
	fm = fimath('RoundingMethod', 'Convergent',...
	     'OverflowAction', 'Wrap',...
	     'ProductMode','FullPrecision',...
	     'SumMode','FullPrecision');
end
