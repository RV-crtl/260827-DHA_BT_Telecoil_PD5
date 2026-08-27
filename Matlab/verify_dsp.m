function verify_dsp()
%VERIFY_DSP Independent MATLAB verification of portable DSP algorithms.
%
% This script reproduces the equations and constants implemented by the C
% application modules without calling the firmware itself. It provides an
% independent numerical cross-check of:
%   1) telecoil two-biquad frequency response at 16 kHz and 48 kHz;
%   2) telecoil DC and impulse stability criteria used by the unit tests;
%   3) audio_dynamics DC blocker, block AGC, smoothing and limiter behaviour;
%   4) Q15 gain/saturation arithmetic used by audio_processing;
%   5) the 20 dB RMS signal-quality decision boundary.
%
% No specialised MATLAB toolbox is required. Figures are written to
% MATLAB/results so they can be retained as supplementary verification
% evidence if desired.

clc;
close all;

root = fileparts(mfilename('fullpath'));
results_dir = fullfile(root, 'results');
if ~exist(results_dir, 'dir')
    mkdir(results_dir);
end

fprintf('=== Independent MATLAB DSP verification ===\n');

%% 1. Telecoil filter frequency response
rates = [16000, 48000];

for fs = rates
    sos = telecoil_coefficients(fs);
    f = logspace(log10(10), log10(fs/2), 5000);
    H = cascade_response(sos, fs, f);
    mag_db = 20*log10(max(abs(H), 1e-12));

    figure('Name', sprintf('Telecoil filter %d Hz', fs));
    semilogx(f, mag_db, 'LineWidth', 1.4);
    grid on;
    xlabel('Frequency (Hz)');
    ylabel('Magnitude (dB)');
    title(sprintf('Telecoil filter frequency response, f_s = %d Hz', fs));
    xlim([10 fs/2]);
    ylim([-80 10]);
    yline(0, '--');
    exportgraphics(gcf, fullfile(results_dir, sprintf('telecoil_filter_%dkHz.png', fs/1000)), 'Resolution', 180);

    p50 = abs(cascade_response(sos, fs, 50)).^2;
    p1k = abs(cascade_response(sos, fs, 1000)).^2;

    assert(p1k > 0.81 && p1k < 1.21, ...
        '1 kHz pass-band power ratio failed at %d Hz.', fs);
    assert(p50 < 0.0016, ...
        '50 Hz rejection failed at %d Hz.', fs);

    if fs == 16000
        phigh = abs(cascade_response(sos, fs, 7000)).^2;
        assert(phigh < 0.01, '7 kHz rejection failed at 16 kHz.');
    else
        phigh = abs(cascade_response(sos, fs, 12000)).^2;
        assert(phigh < 0.0144, '12 kHz rejection failed at 48 kHz.');
    end

    fprintf('Telecoil filter %d Hz: PASS\n', fs);
end

%% 2. Telecoil DC decay and impulse stability (matches C test intent)
fs = 48000;
sos = telecoil_coefficients(fs);

step_in = ones(4000, 1);
step_out = run_biquad_cascade(sos, step_in);
assert(abs(step_out(end)) < 0.001, 'Telecoil DC decay criterion failed.');

impulse = [1; zeros(3000, 1)];
impulse_out = run_biquad_cascade(sos, impulse);
assert(abs(impulse_out(end)) < 1e-4, 'Telecoil impulse stability criterion failed.');

figure('Name', 'Telecoil transient verification');
tiledlayout(2,1);
nexttile;
plot(step_out, 'LineWidth', 1.2);
grid on;
xlabel('Sample'); ylabel('Amplitude');
title('Telecoil filter response to DC input');
nexttile;
plot(impulse_out, 'LineWidth', 1.2);
grid on;
xlabel('Sample'); ylabel('Amplitude');
title('Telecoil filter impulse response');
exportgraphics(gcf, fullfile(results_dir, 'telecoil_transients.png'), 'Resolution', 180);
fprintf('Telecoil DC/impulse stability: PASS\n');

%% 3. Q15 gain and saturation arithmetic
samples = int16([-32768, -20000, -1000, 0, 1000, 20000, 32767]);
for gain_q15 = int32([16384, 32768, 65536])
    y = arrayfun(@(x) apply_gain_q15(x, gain_q15), samples);
    ideal = double(samples) .* (double(gain_q15)/32768);
    ideal = min(max(round_away_from_zero_half(ideal), -32768), 32767);
    assert(all(double(y) == ideal), 'Q15 gain arithmetic mismatch.');
end
assert(apply_gain_q15(int16(1234), int32(-1)) == 0, ...
    'Negative-gain rejection mismatch.');
fprintf('Q15 gain and saturation arithmetic: PASS\n');

%% 4. Audio dynamics cross-check using the exact C defaults
cfg.dc_alpha_q15 = int32(32604);
cfg.target_peak = int32(24000);
cfg.min_gain_q15 = int32(8192);
cfg.max_gain_q15 = int32(65536);
cfg.attack_shift = 1;
cfg.release_shift = 4;

fs = 16000;
t = (0:1/fs:1.2-1/fs).';
% Deliberately includes DC offset and three amplitude regions so the AGC is
% forced to attenuate and recover while preserving deterministic behaviour.
x = 3500 + ...
    7000*sin(2*pi*700*t) .* (t < 0.40) + ...
    30000*sin(2*pi*700*t) .* (t >= 0.40 & t < 0.80) + ...
    5000*sin(2*pi*700*t) .* (t >= 0.80);
x = int16(min(max(round(x), -32768), 32767));

block_size = 128;
[y, gain_trace] = run_audio_dynamics_c(cfg, x, block_size);

assert(max(abs(double(y))) <= double(cfg.target_peak), ...
    'Audio dynamics limiter exceeded target peak.');
assert(abs(mean(double(y(end-999:end)))) < abs(mean(double(x(end-999:end)))), ...
    'DC blocker did not reduce residual DC.');
assert(min(gain_trace) < 32768, 'AGC did not attenuate the loud region.');
assert(gain_trace(end) > min(gain_trace), 'AGC did not recover after attenuation.');

figure('Name', 'Audio dynamics verification');
tiledlayout(2,1);
nexttile;
plot(t, double(x), 'LineWidth', 0.8); hold on;
plot(t, double(y), 'LineWidth', 0.8);
grid on;
xlabel('Time (s)'); ylabel('PCM counts');
title('Audio dynamics input/output');
legend('Input', 'Output', 'Location', 'best');
nexttile;
block_time = ((0:numel(gain_trace)-1) * block_size) / fs;
plot(block_time, double(gain_trace)/32768, 'LineWidth', 1.2);
grid on;
xlabel('Time (s)'); ylabel('Gain (x)');
title('Block AGC gain trajectory');
exportgraphics(gcf, fullfile(results_dir, 'audio_dynamics.png'), 'Resolution', 180);
fprintf('Audio dynamics DC blocker / AGC / limiter: PASS\n');

%% 5. 20 dB RMS signal-quality criterion
noise = 1000;
ratio = linspace(0.1, 20, 1000);
signal = ratio * noise;
snr_db = 20*log10(ratio);
integer_class = signal >= 10*noise;
analytic_class = snr_db >= 20;
assert(all(integer_class == analytic_class), ...
    '20 dB integer classifier disagrees with analytic RMS criterion.');
assert((1000 >= 10*100) && ~(999 >= 10*100), ...
    'Exact 20 dB boundary check failed.');

figure('Name', '20 dB RMS classification');
semilogx(ratio, snr_db, 'LineWidth', 1.4); hold on;
yline(20, '--');
xline(10, '--');
grid on;
xlabel('RMS amplitude ratio S/N');
ylabel('20 log_{10}(S/N) (dB)');
title('Equivalence of 20 dB criterion and 10:1 RMS amplitude ratio');
exportgraphics(gcf, fullfile(results_dir, 'signal_quality_20dB.png'), 'Resolution', 180);
fprintf('20 dB RMS signal-quality criterion: PASS\n');

fprintf('\nALL MATLAB DSP CHECKS PASSED\n');
fprintf('Figures written to: %s\n', results_dir);
end

function sos = telecoil_coefficients(fs)
% Rows are [b0 b1 b2 a1 a2] and exactly mirror telecoil_filter.c.
if fs == 16000
    sos = [ ...
        0.37827911,  0.75655823,  0.37827911,  0.42877652, 0.22427533; ...
        1.00000000, -2.00000000,  1.00000000, -1.83373199, 0.84753178];
elseif fs == 48000
    sos = [ ...
        0.06510831,  0.13021662,  0.06510831, -1.17296298, 0.44398134; ...
        1.00000000, -2.00000000,  1.00000000, -1.94510494, 0.94677483];
else
    error('Unsupported sample rate.');
end
end

function H = cascade_response(sos, fs, f)
w = 2*pi*double(f)/double(fs);
z1 = exp(-1j*w);
z2 = z1.^2;
H = ones(size(w));
for k = 1:size(sos,1)
    b0 = sos(k,1); b1 = sos(k,2); b2 = sos(k,3);
    a1 = sos(k,4); a2 = sos(k,5);
    H = H .* ((b0 + b1*z1 + b2*z2) ./ (1 + a1*z1 + a2*z2));
end
end

function y = run_biquad_cascade(sos, x)
y = double(x(:));
for stage = 1:size(sos,1)
    b0 = sos(stage,1); b1 = sos(stage,2); b2 = sos(stage,3);
    a1 = sos(stage,4); a2 = sos(stage,5);
    x1 = 0; x2 = 0; y1 = 0; y2 = 0;
    out = zeros(size(y));
    for n = 1:numel(y)
        xn = y(n);
        yn = b0*xn + b1*x1 + b2*x2 - a1*y1 - a2*y2;
        x2 = x1; x1 = xn;
        y2 = y1; y1 = yn;
        out(n) = yn;
    end
    y = out;
end
end

function out = apply_gain_q15(sample, gain_q15)
if gain_q15 < 0
    out = int16(0);
    return;
end
scaled = int64(sample) * int64(gain_q15);
if scaled >= 0
    rounded = scaled + int64(16384);
else
    rounded = scaled - int64(16384);
end
value = fix(double(rounded)/32768); % C signed integer division truncates toward zero.
value = min(max(value, -32768), 32767);
out = int16(value);
end

function y = round_away_from_zero_half(x)
% Mirrors the C Q15 helper's explicit +/- 0.5 LSB bias before truncation.
y = zeros(size(x));
pos = x >= 0;
y(pos) = floor(x(pos) + 0.5);
y(~pos) = ceil(x(~pos) - 0.5);
end

function [out, gain_trace] = run_audio_dynamics_c(cfg, x, block_size)
% Integer model that mirrors AudioDynamics_ProcessBlock() semantics.
x = int16(x(:));
out = zeros(size(x), 'int16');
current_gain = int32(32768);
dc_x_prev = int32(0);
dc_y_prev = int32(0);
num_blocks = ceil(numel(x)/block_size);
gain_trace = zeros(num_blocks, 1, 'int32');

for b = 1:num_blocks
    first = (b-1)*block_size + 1;
    last = min(b*block_size, numel(x));
    block = x(first:last);
    peak = max(abs(double(block)));

    if peak == 0
        target = cfg.max_gain_q15;
    else
        target64 = fix((double(cfg.target_peak)*32768)/peak);
        target64 = min(max(target64, double(cfg.min_gain_q15)), double(cfg.max_gain_q15));
        target = int32(target64);
    end

    if target < current_gain
        shift = cfg.attack_shift;
    else
        shift = cfg.release_shift;
    end
    current_gain = smooth_gain_c(current_gain, target, shift);
    gain_trace(b) = current_gain;

    for n = first:last
        xi = int32(x(n));
        feedback = int64(cfg.dc_alpha_q15) * int64(dc_y_prev);
        feedback_q15 = int32(fix(double(feedback)/32768));
        yi = xi - dc_x_prev + feedback_q15;
        dc_x_prev = xi;
        dc_y_prev = yi;

        scaled = int64(yi) * int64(current_gain);
        sample = int32(fix(double(scaled)/32768));
        sample = min(max(sample, -cfg.target_peak), cfg.target_peak);
        out(n) = int16(sample);
    end
end
end

function updated = smooth_gain_c(current, target, shift)
if shift == 0
    updated = target;
    return;
end
% arm-none-eabi-gcc performs an arithmetic right shift for signed int32_t.
delta = int32(target - current);
step = floor(double(delta) / 2^double(shift));
updated = int32(double(current) + step);
end
