# 260827-DHA_BT_Telecoil_PD5

[![260827-DHA_BT_Telecoil_PD5 verification](https://github.com/RV-crtl/260827-DHA_BT_Telecoil_PD5/actions/workflows/ci.yml/badge.svg)](https://github.com/RV-crtl/260827-DHA_BT_Telecoil_PD5/actions/workflows/ci.yml)

Self-contained STM32CubeIDE project for the Bluetooth and telecoil application contribution, targeting the **NUCLEO-F446RE / STM32F446RE**.

The assessed `Application/` layer is hardware-independent. It contains no STM32 HAL calls, device-register access or dynamic allocation. The small `Board/` layer is used only to run the portable application and tests on the F446.

## Quick start

1. In STM32CubeIDE select **File -> Import -> General -> Projects from Folder or Archive**.
2. Choose the submission ZIP and import `260827-DHA_BT_Telecoil_PD5`.
3. Do **not** regenerate code from the `.ioc`; the supplied project already contains the custom build configurations.
4. Build and run the three configurations:

| Configuration | Purpose | Successful result |
|---|---|---|
| **Debug** | Deterministic application demonstration | Completes with `deadline_misses=0`; LD2 on |
| **Testing** | Primary automated unit-test suite | `256 Tests 0 Failures 0 Ignored` / `OK`; LD2 on |
| **SelfTest** | Supplementary independent regression checks | `52 checks, 0 failures, OK`; LD2 on |

USART2/ST-LINK VCP output is **115200 baud, 8-N-1**. Exact verification and GitHub steps are in [`VERIFICATION.md`](VERIFICATION.md).

## Application modules

| Module | Responsibility |
|---|---|
| `connectivity_controller` | M1-M4 state machine, Bluetooth > telecoil priority, fault/reconnect/recovery/idle timing |
| `connectivity_service` | Top-level orchestration, source processing, diagnostics and timing contract |
| `connectivity_actions` | Injected connect/reconnect/low-power callbacks for mocks or later integration |
| `control_protocol` | Bounded mode/source command parser |
| `bt_profile` | BT401 configuration sequence through injected send/delay callbacks |
| `audio_processing` | PCM conversion, Q15 gain, stereo-to-mono and mute |
| `pcm_transport` | Portable packed/interleaved PCM framing helpers |
| `audio_dynamics` | DC rejection, bounded AGC smoothing and peak limiting |
| `telecoil_detector` | RMS/mean-absolute/peak-to-peak/clipping metrics with hysteresis |
| `telecoil_filter` | Stateful two-biquad conditioning at 16 kHz and 48 kHz |
| `signal_quality` | Integer/fixed-point 20 dB RMS signal-to-noise classification |

## Verification

The project provides:

- **256 automated unit tests**;
- **52 supplementary framework-independent checks**;
- a **100,000-step deterministic state-machine stress test**;
- GCC and Clang host builds;
- AddressSanitizer and UndefinedBehaviourSanitizer;
- GCC static analysis;
- executable-line coverage with a **98% minimum gate**;
- mutation testing of the Bluetooth-priority rule;
- CMake/CTest host execution;
- GitHub Actions with GCC, Clang and GNU Arm Cortex-M4 jobs.

The latest measured application line coverage is **99.22% (888/895 executable lines)**.

Run the complete host verification with:

```bash
make -f Makefile.host verify
```

or CMake/CTest with:

```bash
cmake --preset host-debug
cmake --build --preset host-debug
ctest --preset host-debug
```

## Requirements boundary

[`REQUIREMENTS_TRACEABILITY.md`](REQUIREMENTS_TRACEABILITY.md) maps requirements 1.1-1.20 to the portable implementation and automated evidence. Physical I2S timing, true Bluetooth-to-output latency, measured telecoil SNR and idle power remain system-integration measurements rather than software-only claims.

Design details are summarised in [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md).

## Project layout

```text
Application/        Portable application logic
Board/              Minimal STM32F446RE execution adapters
Core/               Entry point, startup and bare-metal Newlib stubs
Tests/              Primary 256-test suite
IndependentTests/   Supplementary 52-check suite
Host/               Host runners and deterministic stress test
Scripts/            Coverage and mutation helpers
Matlab/             Supporting DSP validation
.github/workflows/  Continuous-integration workflow
docs/               Architecture notes
```

## References

1. STMicroelectronics, *RM0390 — STM32F446xx advanced Arm-based
   32-bit MCUs Reference Manual*. Used for the thin `Board/` and
   startup-layer register definitions and peripheral configuration.

2. STMicroelectronics, *UM1724 — STM32 Nucleo-64 boards (MB1136)
   User Manual*. Used for NUCLEO-F446RE board pin mapping, LD2 and
   ST-LINK virtual COM-port context.

3. *BT401 Bluetooth Audio Module User Manual, Version 1.6*.
   Manufacturer documentation used for the serial framing and
   AT-command meanings represented by `bt_profile`.

4. ThrowTheSwitch, *Unity — A Test Framework for C*. The project
   follows the Unity-style C test API and assertion syntax; the
   compact files under `Tests/Unity/` are a local course-aligned
   implementation rather than a copy of the current upstream Unity
   source distribution.

5. MathWorks, *MATLAB R2025a*. Used only for independent offline
   verification of the implemented DSP equations and frequency
   response; MATLAB is not a runtime dependency of the embedded
   application.

## Acknowledgements

Selected high-level Bluetooth/audio-processing ideas were refactored
from my earlier digital-hearing-aid prototype into hardware-independent
modules for this submission. No STM32 HAL-dependent application logic
was carried into the assessed `Application/` layer.

Generative AI (OpenAI ChatGPT, GPT-5.6 Sol) was used during development
for software-architecture discussion, C-code drafting and refactoring,
unit-test generation, debugging of build/linker issues, host/CI
verification setup, MATLAB verification-script drafting, and
documentation review.

All generated or adapted material was manually reviewed and validated
through clean STM32CubeIDE builds, execution on the NUCLEO-F446RE,
automated unit tests, independent regression checks, host testing,
sanitizers, static analysis, coverage, mutation testing and GitHub
Actions. Final engineering decisions and responsibility for the
submitted work remain with the author.
