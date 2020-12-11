clearvars;close all;clc;
% [yVal] = frexp_log2(init_struc())
[F,E] = frexp_log2(init_struc())
% figplot(yVal)
figplot(F,E)