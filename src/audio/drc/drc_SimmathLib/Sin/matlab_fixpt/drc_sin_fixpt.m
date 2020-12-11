function y = drc_sin_fixpt(x)
fm = get_fimath();

y = fi(sin(x), 1, 32, 31, fm);
end


function fm = get_fimath()
	fm = fimath('RoundingMethod', 'Convergent',...
	     'OverflowAction', 'Wrap',...
	     'ProductMode','FullPrecision',...
	     'SumMode','FullPrecision');
end
