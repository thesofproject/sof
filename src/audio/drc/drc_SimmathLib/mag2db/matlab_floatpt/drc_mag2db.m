function [ydB] = drc_mag2db(tstruct)
    s_lin = tstruct.x;           % in data
    s_lin(isnan(s_lin)) = [];    % Remove NaN   y(y<0) = NaN;
    ydB = zeros(1,size(s_lin,2));% init out ydB        
    qsb  = fixed.Quantizer(numerictype(1,32,24)); % Quantize
    ydB  = quantize(qsb,20*fi(log(complex(double(s_lin))),1,32,24)/fi(log(10),1,32,24));   %ydb = 20*log10(y);
end