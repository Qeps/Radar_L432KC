%% HB100 IF simulator for STM32F303RE DAC (DMA, circular)
clear; clc;

%% ===== Parametry docelowe pod MCU =====
fs      = 1000;     % [Hz] – TIM trigger
T       = 3.0;      % [s]  – bufor zapętlany
N       = round(fs*T);

vref    = 3.3;      % DAC reference
v_mid   = 1.65;     % środek
dac_bits= 12;

%% ===== Fizyka HB100 =====
fc = 10.525e9;
c  = 299792458;

v_mean = 1.0;       % m/s
stepHz = 1.8;
v_step = 0.25;

A0    = 0.4;        % amplituda IF (po wzmocnieniu)
noise = 0.03;

rng(1);

%% ===== Oś czasu =====
t = (0:N-1).' / fs;

%% ===== Profil prędkości =====
gate = double(t > 0.3 & t < (T-0.3));
gate = smoothdata(gate,"gaussian",round(0.05*fs));

v = gate .* ( ...
    v_mean + ...
    v_step*sin(2*pi*stepHz*t) ...
    );

%% ===== Doppler =====
fd = (2*fc/c) * v;      % Hz
phase = 2*pi*cumsum(fd)/fs;
phase = phase + 2*pi*rand;

%% ===== IF =====
x = A0 * cos(phase);
x = x + noise*randn(N,1);

%% ===== Mapowanie na DAC =====
scale = 0.6;            % ręcznie dobrane
y = v_mid + scale*x;

y = min(max(y,0),vref);
codes = uint16(round((2^dac_bits - 1) * y / vref));

%% ===== Eksport =====
fid = fopen("hb100_dac_buf.h","w");
fprintf(fid,"#pragma once\n#include <stdint.h>\n");
fprintf(fid,"#define HB100_FS_HZ %d\n",fs);
fprintf(fid,"#define HB100_BUF_LEN %d\n\n",N);
fprintf(fid,"static const uint16_t hb100_buf[HB100_BUF_LEN] = {\n");

for i=1:N
    if mod(i-1,16)==0, fprintf(fid,"  "); end
    fprintf(fid,"%d",codes(i));
    if i<N, fprintf(fid,", "); end
    if mod(i,16)==0, fprintf(fid,"\n"); end
end
fprintf(fid,"\n};\n");
fclose(fid);

disp("OK: hb100_dac_buf.h");

%% ===== Wykresy kontrolne =====
figure;

subplot(3,1,1);
plot(t, v);
grid on;
ylabel("v [m/s]");
title("Profil prędkości celu");

subplot(3,1,2);
plot(t, fd);
grid on;
ylabel("f_d [Hz]");
title("Częstotliwość Dopplera");

subplot(3,1,3);
plot(t, x);
grid on;
ylabel("IF [V]");
xlabel("t [s]");
title("Sygnał IF (przed mapowaniem na DAC)");

figure;
plot(t, y);
grid on;
xlabel("t [s]");
ylabel("V");
title("Wyjście DAC (0–3.3 V)");
