function y = drc_asin_fixpt(x)
fm = get_fimath();

y = fi(asin(x), 1, 32, 30, fm);
end


function fm = get_fimath()
	fm = fimath('RoundingMethod', 'Convergent',...
	     'OverflowAction', 'Wrap',...
	     'ProductMode','FullPrecision',...
	     'SumMode','FullPrecision');
end
