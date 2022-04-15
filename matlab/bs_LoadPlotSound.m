%% bs_LoadPlotSound
% Load a text file containing stereo audio data
% Assumption: audio collected with packet counter and timestamps.
% Assumes the packet counter is in column 1 (first column) and channel is
% in column 3.
%
% Input:
%   fname:      full path to filename
%   sr:         sample rate
% Output:
%   stereo:     nx2 matrix with stereo sound

function stereo=bs_LoadPlotSound(fname,sr);

    fprintf(1,'Loading %s\n',fname);
    raw = load(fname);
    fprintf(1,'Loaded matrix of %d x %d\n',size(raw,1),size(raw,2));
    
    % Decode assuming packet counter in column 1 and channel in column 3.
    data = bsSoundFrameToSample(raw,1,3);
    stereo = data;
    
    % Plot data
    figure;    
    plot(data(:,1),'b-');
    hold on;
    plot(data(:,2),'r-');

    title(['File name: ' fname]);

    % Play the sound
    soundsc(stereo,sr)

    
end
