function y = drc_log_fixpt(x)
fm = get_fimath();

y = fi(log(complexx(double(x))), 0, 32, 30, fm);
end



function c = complexx(varargin)
    coder.inline( 'always' );
    if nargin==2
        re = varargin{ 1 };
        im = varargin{ 2 };
        if isfi( re ) && isfi( im )
            % Choose wider type
            NT = fixed.aggregateType( re, im );
            c = complex( fi( re, NT ), fi( im, NT ) );
        else
            c = complex( re, im );
        end
    elseif nargin==1
        c = complex( varargin{ 1 } );
    end
end

function fm = get_fimath()
	fm = fimath('RoundingMethod', 'Convergent',...
	     'OverflowAction', 'Wrap',...
	     'ProductMode','FullPrecision',...
	     'SumMode','FullPrecision');
end
