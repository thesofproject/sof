function eq_biquad_adj_coefs = eq_adjust_shift_gain(eq_biquad_coefs);

eq_biquad_adj_coefs = cell(1, length(eq_biquad_coefs));
total_shift = 0;
total_gain = 1.0;
for i = 1:length(eq_biquad_coefs)
	coef = cell2mat(eq_biquad_coefs(i));

	% Biquad coefficients are Q2.30 (range:(-2,2))
	% If a2(coef(1)) and a1(coef(2)) are out of range, there is no way to
	% adjust. Make an assertion then.
	if (abs(coef(1)) >= 2.0 || abs(coef(2)) >= 2.0)
		error("Invalid biquad coef: a2=%f a1=%f", coef(1), coef(2));
	endif

	% Adjust the absolute values of b2,b1,b0(coef(3:5)) to not greater than
        % 1.0, and restore the intensity by gain(coef(7))
	max_b = max(max(abs(coef(3)), abs(coef(4))), abs(coef(5)));
	if (max_b > 1.0)
		coef(3) /= max_b;
		coef(4) /= max_b;
		coef(5) /= max_b;
		coef(7) *= max_b;
	endif

	% Postpone shifts and gains of all former biquad stages to the last
	% stage, in order to keep inner responses un-saturated.
	total_shift += coef(6);
	total_gain *= coef(7);
	if (i == length(eq_biquad_coefs))
		% total_gain is Q2.14 (range:(-2,2)). Use total_shift to adjust
		% total_gain until it is within range.
		while (abs(total_gain) >= 2.0)
			total_gain /= 2;
			total_shift--;
		endwhile

		coef(6) = total_shift;
		coef(7) = total_gain;
	else
		coef(6) = 0;
		coef(7) = 1.0;
	endif

	eq_biquad_adj_coefs(i) = coef;
endfor

endfunction
