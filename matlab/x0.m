%% Load and play audio logs
% Audio acquired with: 
%   8KHz stereo: S,1,2,1,10
%   16KHz stereo: S,2,2,2,10
%   20KHz stereo: S,3,2,3,10

fprintf(1,'Loading 8KHz log\n');
bs_LoadPlotSound('LOG-0000.001',8000);
pause(11);

fprintf(1,'Loading 16KHz log\n');
bs_LoadPlotSound('LOG-0000.002',16000);
pause(11);

fprintf(1,'Loading 20KHz log\n');
bs_LoadPlotSound('LOG-0000.003',20000);
pause(11);

