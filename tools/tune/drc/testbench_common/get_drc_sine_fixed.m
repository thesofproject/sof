%%---------------------------------------------------------------------
%  SPDX-License-Identifier: BSD-3-Clause
%
%  Copyright(c) 2020 Intel Corporation. All rights reserved.
%
%  Author: Shriram Shastry <malladi.sastry@linux.intel.com>
%% Initialize variables.
function drcsinfixed = get_drc_sine_fixed(path,filename)
%---------------------------------------------------
%---------------------------------------
%   History
%---------------------------------------
%   2020/12/24 Sriram Shastry       - initial version
%
%% Initialize variables.

Info = dir(fullfile(path));
Info = dir(fullfile(filename));
fullFileName            = fullfile(Info.folder,Info.name);
if exist(fullFileName, 'file')
startRow = 2;

%% Format for each line of text:
%   column1: double (%f)
%	column2: double (%f)
%   column3: double (%f)
% For more information, see the TEXTSCAN documentation.
formatSpec = '%11f%12f%f%[^\n\r]';

%% Open the text file.
fileID = fopen(filename,'r');

%% Read columns of data according to the format.
dataArray = textscan(fileID, formatSpec, 'Delimiter', '', 'WhiteSpace', '', 'TextType', 'string', 'HeaderLines' ,startRow-1, 'ReturnOnError', false, 'EndOfLine', '\r\n');

%% Close the text file.
fclose(fileID);

%% Create output variable
drcsinfixed = table;
drcsinfixed.idx = dataArray{:, 1};
drcsinfixed.insine = dataArray{:, 2};
drcsinfixed.outsine = dataArray{:, 3};

%% Clear temporary variables
clearvars filename startRow formatSpec fileID dataArray ans;
else
    % File does not exist.  user warning.
    errorMessage = sprintf('Error: file not found:\n\n%s', fullFileName);
    uiwait(errordlg(errorMessage));
    return;
end
disp(fullFileName);
