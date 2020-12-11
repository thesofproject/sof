
function y = drc_floor_fixpt(x)
fm = get_fimath();

y = fi(floor(x), 0, 1, 0, fm)
end


function fm = get_fimath()
	fm = fimath('RoundingMethod', 'Convergent',...
	     'OverflowAction', 'Wrap',...
	     'ProductMode','FullPrecision',...
	     'SumMode','FullPrecision');
end
