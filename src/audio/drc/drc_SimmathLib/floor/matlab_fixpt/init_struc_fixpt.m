
function x = init_struc_fixpt ()
fm = get_fimath();

x = fi([1.546565565:0.1:2], 0, 32, 31, fm);
end


function fm = get_fimath()
	fm = fimath('RoundingMethod', 'Convergent',...
	     'OverflowAction', 'Wrap',...
	     'ProductMode','FullPrecision',...
	     'SumMode','FullPrecision');
end
