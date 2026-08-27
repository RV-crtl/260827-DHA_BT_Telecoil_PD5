# Architecture and Design Notes

## Layering

<img width="1448" height="860" alt="architecture" src="https://github.com/user-attachments/assets/58a4471a-186e-4251-bfb8-15e8f97887f3" />


`Application/` owns policy and DSP only. `Board/` contains the minimal STM32F446RE-specific console and optional BT401 UART adapter.

## State model

System state and audio-source identity are separate:

- **M1 Initialising** — startup/interface readiness window;
- **M2 Active** — a valid source is selected and audio can be routed;
- **M3 Idle** — no active source; low-power entry may be requested;
- **M4 Fault** — active-source failure/timeout; output is muted while recovery is attempted.

The arbiter is deterministic: **Bluetooth has priority over telecoil** whenever both are enabled and valid. Intentional source disable is treated as control input rather than as a hardware failure.

All millisecond elapsed-time calculations use unsigned `uint32_t` subtraction. This naturally handles one timer rollover; public APIs reject timestamps that appear more than half the 32-bit range backwards, assuming calls occur less than 2^31 ms apart.

## Timing contract

The portable application is scheduler-independent, so timing requirements are expressed as an integration contract:

- maximum intended service interval: **40 ms**;
- telecoil-loss confirmation: **40 ms**;
- conservative telecoil-fault software bound: **<=80 ms**;
- stable-source recovery debounce: **100 ms**;
- Bluetooth stale-frame guard: **100 ms**.

`ConnectivityDiagnostics_t` records maximum observed service interval and deadline misses. The controller enters M4 and asserts mute in the same update that receives an invalid active source.

## PCM and fixed-point processing

`audio_processing` and `pcm_transport` preserve useful audio-path concepts without depending on SAI/I2S hardware:

- signed 24-bit PCM in bits [23:0] is sign-extended and shifted to S16;
- Q15 uses **32768 = 1.0x** gain;
- gain multiplication uses widened intermediates and saturates rather than wrapping;
- stereo fold uses `(L + R) / 2` with a 32-bit intermediate;
- packed little-endian S24 and interleaved stereo words are decoded at the portable boundary.

## Audio dynamics

`audio_dynamics` implements a deterministic first-order DC blocker:

```text
y[n] = x[n] - x[n-1] + alpha*y[n-1]
```

where `alpha` is stored in Q15. Block peak amplitude determines a target gain:

```text
target_gain_q15 = target_peak * 32768 / measured_peak
```

The gain is clamped to configured minimum/maximum values and smoothed with separate attack and release shifts. Faster attenuation protects against sudden loud blocks; slower recovery reduces audible gain pumping. A final limiter bounds output magnitude.

## Telecoil detector and filter

The detector calculates integer RMS, mean absolute magnitude, peak-to-peak excursion and clipping incidence. Separate present/absent thresholds plus consecutive-block hysteresis reduce chatter. Threshold values are configurable calibration defaults; later physical measurements can tune them without changing the API.

The filter is two cascaded second-order IIR sections. Each direct-form-I stage implements:

```text
y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2]
       - a1*y[n-1] - a2*y[n-2]
```

Separate precomputed coefficient sets are loaded for 16 kHz and 48 kHz to provide useful speech-band conditioning around the 300 Hz to 5 kHz region. Tests check pass-band response, stop-band attenuation, DC decay, impulse stability and state continuity.

## 20 dB signal-quality criterion

For RMS amplitudes:

```text
20*log10(signal/noise) >= 20 dB
```

is equivalent to:

```text
signal/noise >= 10
```

`signal_quality` therefore performs the classification with integer arithmetic and a widened `noise * 10` comparison instead of requiring `log10()` or `libm`. A separate Q8 helper represents a ratio using 256 = 1.0.

## Control and integration boundary

`control_protocol` accepts bounded case-insensitive frames:

```text
BT ON / BT OFF
TC ON / TC OFF
ALL ON / ALL OFF
MODE AUTO / MODE BT / MODE TC / MODE OFF
STATUS
```

`connectivity_actions` converts controller request flags into injected callbacks for Bluetooth connect/reconnect and low-power entry/exit. A later hardware/RTOS adapter can implement those callbacks; tests use mocks/spies.
