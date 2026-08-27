# Verification Workflow

## 1. STM32CubeIDE import

1. Open STM32CubeIDE in a clean workspace where practical.
2. Select **File -> Import -> General -> Projects from Folder or Archive**.
3. Choose the submission ZIP and import `260827-DHA_BT_Telecoil_PD5`.
4. If the editor shows stale include markers, right-click the project -> **Index -> Rebuild**.
5. Do **not** use CubeMX **Generate Code** from the `.ioc`.
6. Connect the NUCLEO-F446RE through ST-LINK USB and open the **STMicroelectronics STLink Virtual COM Port** at **115200, 8-N-1, no flow control**.

## 2. Debug application

1. **Project -> Build Configurations -> Set Active -> Debug**.
2. **Project -> Clean**, then **Build Project**.
3. Require a clean build with no errors.
4. Run `Debug/260827-DHA_BT_Telecoil_PD5.elf`.

The demonstration should show the following behaviour:

```text
Telecoil detected -> M2 Active / Telecoil
Bluetooth becomes valid -> M2 Active / Bluetooth
101 ms Bluetooth frame -> rejected
Bluetooth link lost -> M4 Fault / MUTED
Stable telecoil recovery -> M2 Active / Telecoil
deadline_misses=0
max_service_interval_ms=40
```

LD2 turns on at successful completion.

## 3. Primary automated unit tests

1. **Set Active -> Testing**.
2. **Clean -> Build Project**.
3. Run `Testing/260827-DHA_BT_Telecoil_PD5.elf`.
4. Confirm the final summary:

```text
-----------------------
256 Tests 0 Failures 0 Ignored
OK

Unity Testing build finished. Green LED = all tests passed.
```

The test suite covers the eleven portable application modules plus the explicit timing contract. It includes state-machine sequences, exact timeout boundaries, invalid inputs, timer rollover, mocked callbacks/transport, PCM and fixed-point processing, audio dynamics, telecoil detection/filtering and signal-quality classification.

## 4. Supplementary SelfTest

1. **Set Active -> SelfTest**.
2. **Clean -> Build Project**.
3. Run `SelfTest/260827-DHA_BT_Telecoil_PD5.elf`.
4. Confirm:

```text
SELFTEST RESULT: 52 checks, 0 failures, OK
Framework-free SelfTest build finished. Green LED = all checks passed.
```

SelfTest is a separate regression check over key behaviours; it does not replace the primary 256-test suite.

## 5. Host verification

The hardware-independent application can also be tested on a host PC:

```bash
make -f Makefile.host verify
```

This runs:

- the 256-test suite;
- the 52-check SelfTest;
- the 100,000-step deterministic stress test;
- AddressSanitizer and UndefinedBehaviourSanitizer;
- GCC static analysis;
- strict embedded-source syntax builds;
- executable-line coverage with a 98% minimum gate;
- mutation testing that deliberately reverses Bluetooth/telecoil priority and requires the tests to detect it.

CMake/CTest provides a second host build path:

```bash
cmake --preset host-debug
cmake --build --preset host-debug
ctest --preset host-debug
```

Expected targets:

```text
unit_tests_256
independent_selftest_52
deterministic_fuzz_100k
```

## 6. GitHub Actions

The repository contains `.github/workflows/ci.yml`.

1. Copy the final project contents into the existing local Git repository while preserving `.git`.
2. Commit the changes and push `main`.
3. Open **GitHub -> Actions -> 260827-DHA_BT_Telecoil_PD5 verification**.
4. Require all three jobs to be green:

```text
Host verification (GCC)
Host verification (Clang)
Cortex-M4 cross-build
```

The GCC job runs the complete host verification and CMake/CTest. The Clang job checks compiler portability. The Cortex-M4 job independently builds Debug, Testing and SelfTest with GNU Arm Embedded.

## 7. Final evidence

A concise evidence set is sufficient:

- clean CubeIDE build;
- Debug serial output showing priority, fault/mute, recovery and zero deadline misses;
- `256 Tests 0 Failures 0 Ignored`;
- `52 checks, 0 failures`;
- GitHub Actions summary with all three jobs green;
- optionally, one CI excerpt showing coverage, stress and mutation results.

Before submission, import the **exact ZIP that will be uploaded** into a fresh CubeIDE workspace and clean-build Debug, Testing and SelfTest once more.
