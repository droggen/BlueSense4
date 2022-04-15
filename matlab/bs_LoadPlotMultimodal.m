function bs_LoadPlotMultimodal(fname)
[dat_sound,dat_motion,dat_adc] = bsLoadMultimodal(fname);
bsPlotMultimodal(dat_sound,dat_motion,dat_adc);