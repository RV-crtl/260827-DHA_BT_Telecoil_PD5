# Bluetooth + Telecoil Requirements Traceability

This table maps the subsystem requirements to the **portable PD5 application**. Software evidence is separated from measurements that necessarily belong to later hardware integration.

| Req | Portable implementation | Automated evidence | PD5 status |
|---|---|---|---|
| 1.1 initialise <=3 s | M1 startup timeout = 3000 ms | exact 2999/3000 ms tests | Software policy implemented |
| 1.2 BT connection <=5 s | connect request + 5000 ms timeout | 4999/5000 ms tests; injected action port | Software policy implemented |
| 1.3 monitor telecoil M2/M3 | detector runs independently of selected source | idle-to-telecoil service tests | Implemented |
| 1.4 receive/decode BT audio | PCM/S24 helpers + BT block processing | PCM transport + service tests | Portable side implemented; hardware ingress later |
| 1.5 acquire/condition telecoil | metrics, timed qualification, two-biquad filter and gain | detector/filter/service tests | Portable side implemented |
| 1.6 both streams via I2S | hardware-independent PCM framing boundary | PCM transport tests | Physical I2S later |
| 1.7 BT > telecoil | deterministic arbitration | unit, independent, stress and mutation tests | Implemented |
| 1.8 switch <=100 ms | source changes in detecting update; service contract <=40 ms | timing-contract tests | Portable timing supports requirement |
| 1.9 BT latency <=100 ms | frame-age guard rejects >100 ms | exact 100/101 ms tests | Application guard implemented; physical end-to-end measurement later |
| 1.10 telecoil SNR >=20 dB | integer RMS 20 dB classification | exact boundary tests | Classifier implemented; physical SNR later |
| 1.11 16/48 kHz | explicit rate policy and filter coefficient sets | both rates tested | Implemented |
| 1.12 idle <=10 mW | M3 exposes `request_low_power` | controller/action tests | Control intent implemented; power measurement later |
| 1.13 BCLK/LRCLK/SD | portable PCM framing; no I2S/HAL dependency in PD5 | PCM tests | Physical interface later |
| 1.14 mode/enable commands | service commands + bounded text protocol | protocol/service tests | Implemented |
| 1.15 active-source status | `ConnectivityOutputs_t` snapshot | status tests | Implemented |
| 1.16 enter M4 <=100 ms | <=40 ms service interval + 40 ms telecoil-loss confirmation; BT loss on next observation | exact timer/fault tests | Portable worst-case <=80 ms contract |
| 1.17 mute <=50 ms after M4 | mute asserted in same controller transition; intended service interval <=40 ms | M4 mute/timing tests | Portable reaction immediate after detection; physical output buffering later |
| 1.18 reconnect every 2 s | periodic reconnect request | 1999/2000 ms and repeated retry tests | Implemented |
| 1.19 valid source -> M2 <=2 s | 100 ms stable-source recovery | recovery tests | Implemented |
| 1.20 M4 -> M3 after 5 s no input | 5000 ms fault-to-idle timer | 4999/5000 ms tests | Implemented |

The final integrated system must separately measure I2S timing/data integrity, true Bluetooth-to-output latency, physical telecoil SNR and idle power. PD5b deliberately keeps those devices outside the automated application tests.
