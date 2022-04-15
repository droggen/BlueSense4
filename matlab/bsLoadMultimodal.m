%% bsLoadMultimodal
% Load a multimodal file with motion, sound and 1 channel ADC
% Assumes text format, with packet counter and timestamps only.
%
% Input:
%   fname:      full path to file name

function [dat_sound,dat_motion,dat_adc] = bsLoadMultimodal(fname,verbose)
%%
if nargin==1
    verbose=0;
end
%%
dat_sound=[];
dat_motion=[];
dat_adc=[];


%% Read the file in two steps: first parses to know how many lines of each type, then create 
% matrices, then read the data

fid = fopen(fname);
if fid==-1
    fprintf(2,'Cannot open %s\n',fname);
    return
end
n_adc=0;
n_motion=0;
n_sound=0;
fprintf(1,'Scanning %s\n',fname);
tline = fgetl(fid);
while ischar(tline)
    %disp(tline)
    nline = str2num(tline);
    l = length(nline);

    if l==3
        n_adc=n_adc+1;
    end
    if l==15
        n_motion=n_motion+1;
    end
    if l==131
        n_sound=n_sound+1;
    end
    tline = fgetl(fid);    
end

fprintf(1,'Number of sound frames: %d\n',n_sound);
%fprintf(1,'Duration of sound: %d\n',n_sound*128/16000);
fprintf(1,'Number of ADC samples: %d\n',n_adc);
fprintf(1,'Number of motion samples: %d\n',n_motion);
%fprintf(1,'Duration of motion: %d\n',n_motion/225);

%% Resize the buffers
dat_sound=zeros(n_sound,131);
dat_motion=zeros(n_motion,15);
dat_adc=zeros(n_adc,3);

%% Read the file and store in buffers
% Seek back to start
fseek(fid,0,'bof');

fprintf(1,'Reading %s\n',fname);
tline = fgetl(fid);
ti = 0;
ia = 1;
is = 1;
im = 1;
while ischar(tline)
    %disp(tline)
    nline = str2num(tline);
    l = length(nline);
    
    % Check whether the line is:
    % Motion: 15 fields
    % ADC: 3 fields
    % Sound: 131 fields
    
    if l==3
        dat_adc(ia,:)=nline;
        ia = ia+1;
        if verbose==1
            fprintf(1,'A');
        end
    end
    if l==15
        dat_motion(im,:)=nline;
        im=im+1;
        if verbose==1
            fprintf(1,'M');
        end
    end
    if l==131
        dat_sound(is,:)=nline;
        is=is+1;
        if verbose==1
            fprintf(1,'S');
        end
    end
    
    % Newline
    ti = ti+1;
    if verbose==1
        if mod(ti,80)==0        
            fprintf(1,'\n');
        end
    else
        if mod(ti,10000)==0
            fprintf(1,'Read %d lines (%d sound %d motion %d ADC)\n',ti,is,im,ia);
        end
    end
    
    % Early break for debug
%     if ti>500
%         break
%     end
    % Get next line
    tline = fgetl(fid);    
end
fclose(fid);

fprintf(1,'Read completed with %d sound %d motion %d ADC\n',is-1,im-1,ia-1);

end
