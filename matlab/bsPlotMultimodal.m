function bsPlotMultimodal(ds,dm,da)

% Convert the sound frames to stereo
stereo = bsSoundFrameToSample(ds,1,3);

figure;
ax(1) = subplot(3,1,1);
plot(stereo(:,1),'b-');
hold on;
plot(stereo(:,2),'r-');
title('Sound');
ax(2) = subplot(3,1,2);
plot(dm(:,3),'r-');
hold on
plot(dm(:,4),'g-');
plot(dm(:,5),'b-');
title('Acceleration');
ax(3) = subplot(3,1,3);
plot(da(:,3));
title('ADC');

% Play
soundsc(stereo,16000);