function y = drc_round_fixpt(x)

fm = get_fimath();

y = fi(round(x), 0, 3, 0, fm);
end


function fm = get_fimath()
	fm = fimath('RoundingMethod', 'Convergent',...
	     'OverflowAction', 'Wrap',...
	     'ProductMode','FullPrecision',...
	     'SumMode','FullPrecision');
end
