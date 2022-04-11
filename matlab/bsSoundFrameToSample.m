%% bsSoundFrameToSample
%
% Input:
%   frame:          Data in frame format with optional timestamp, packet counter, channel id, and then data.
%   c_channel:      Position of the channel in the frame
%   c_packet:       Position of the packet couner
%
% Returns:
%   nx2 matrix with left/right data
function data=bsSoundFrameToSample(frame,c_packet,c_channel)

% Get the number of samples per frames
ns = size(frame,2)-c_channel;

fl = frame(:,c_channel) == 0;   % Left frames
fr = frame(:,c_channel) == 1;   % Right frames

framel = frame(fl,:);
framer = frame(fr,:);

%% If the one or the other frame is empty, just copy the first packet and
% timestamp.
if size(framel,1) == 0 && size(framer,1) == 0
    fprintf(2,'Error: no audio data for both the left and right channe\n');
	return
end
if size(framel,1)==0
    fprintf(1,'No left audio data: creating zero data for left channel\n');
    % Copy the data of right to left
    framel = framer;
    % Zero all the samples
    framel(:,c_channel+1:end) = zeros(size(framel,1),size(framel,2)-c_channel);
end
if size(framer,1)==0
    fprintf(1,'No right audio data: creating zero data for right channel\n');
    % Copy the data of left to right
    framer = framel;
    % Zero all the samples
    framer(:,c_channel+1:end) = zeros(size(framer,1),size(framer,2)-c_channel);
end

%% Get the smallest packet it and the largest 
pktmin = min(framel(1,c_packet),framer(1,c_packet));
pktmax = max(framel(end,c_packet),framer(end,c_packet));
npkt = pktmax-pktmin+1;

%% Create the result data 
data = zeros(npkt*ns,2);

%% Go through all the dataset
for i=1:size(framel,1)
    p = framel(i,c_packet);         % Packet of frame
    o = p*ns+1;                     % Convert packet to offset in data structure (+1 for 1 based)    
    data(o:o+ns-1,1) = framel(i,c_channel+1:end)';    
end
for i=1:size(framer,1)
    p = framer(i,c_packet);         % Packet of frame
    o = p*ns+1;                     % Convert packet to offset in data structure (+1 for 1 based)    
    data(o:o+ns-1,2) = framer(i,c_channel+1:end)';    
end








