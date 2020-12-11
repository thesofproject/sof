function [tstruct] = init_struc_fixpt ()
fm = get_fimath();

x            = fi((14), 0, 4, 0, fm);
ydB          = fi(zeros(1,size(x,fi(2, 0, 2, 0, fm))), 0, 1, 0, fm);
tstruct.x    = fi((x), 0, 4, 0, fm);
tstruct.ydB  = fi((ydB), 0, 1, 0, fm);
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


function ntype = divideType(a,b)
    coder.inline( 'always' );
    nt1 = numerictype( a );
    nt2 = numerictype( b );
    maxFL = max( [ min( nt1.WordLength, nt1.FractionLength ), min( nt2.WordLength, nt2.FractionLength ) ] );
    FL = max( maxFL, 24 );
    extraBits = (FL - maxFL);
    WL = nt1.WordLength + nt2.WordLength;
    WL = min( WL, 124 );
    if (WL + extraBits)<64
        ntype = numerictype( nt1.Signed || nt2.Signed, WL + extraBits, FL );
    else
        ntype = numerictype( nt1.Signed || nt2.Signed, WL, FL );
    end
end


function c = fi_div(a,b)
    coder.inline( 'always' );
    if isfi( a ) && isfi( b ) && isscalar( b )
        a1 = fi( a, 'RoundMode', 'fix' );
        b1 = fi( b, 'RoundMode', 'fix' );
        c1 = divide( divideType( a1, b1 ), a1, b1 );
        c = fi( c1, numerictype( c1 ), fimath( a ) );
    else
        c = a / b;
    end
end

function fm = get_fimath()
	fm = fimath('RoundingMethod', 'Convergent',...
	     'OverflowAction', 'Wrap',...
	     'ProductMode','FullPrecision',...
	     'SumMode','FullPrecision');
end
