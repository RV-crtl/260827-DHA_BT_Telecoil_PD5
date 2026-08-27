#include "connectivity_controller.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define FUZZ_STEPS 100000U

static uint32_t prng_state = UINT32_C(0xC0FFEE42);

static uint32_t prng_next(void)
{
    uint32_t x = prng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    prng_state = x;
    return x;
}

static void invariant(bool condition, const char *message, uint32_t step)
{
    if (!condition) {
        fprintf(stderr, "FUZZ invariant failed at step %u: %s\n", step, message);
        exit(EXIT_FAILURE);
    }
}

int main(void)
{
    ConnectivityController_t controller;
    const ConnectivityConfig_t config = Connectivity_DefaultConfig();
    ConnectivityInputs_t inputs = {0};
    ConnectivityOutputs_t outputs;
    uint32_t now_ms = UINT32_MAX - UINT32_C(5000);

    if (Connectivity_Init(&controller, &config, now_ms) != CONNECTIVITY_OK) {
        fputs("FUZZ: controller init failed\n", stderr);
        return EXIT_FAILURE;
    }

    for (uint32_t step = 0U; step < FUZZ_STEPS; ++step) {
        const uint32_t random = prng_next();
        now_ms += (random & UINT32_C(0x1F)); /* monotonic modulo-2^32 tick, includes wrap */

        inputs.now_ms = now_ms;
        inputs.interfaces_ready = ((random >> 5) & 1U) != 0U;
        inputs.paired_device_available = ((random >> 6) & 1U) != 0U;
        inputs.bluetooth_connected = ((random >> 7) & 1U) != 0U;
        inputs.telecoil_valid = ((random >> 8) & 1U) != 0U;

        /* Exercise intentional control changes as well as source observations. */
        if ((step % 257U) == 0U) {
            const bool bt_enabled = ((random >> 9) & 1U) != 0U;
            const bool tc_enabled = ((random >> 10) & 1U) != 0U;
            invariant(Connectivity_SetEnabled(&controller, bt_enabled, tc_enabled, now_ms) ==
                          CONNECTIVITY_OK,
                      "enable/disable command rejected",
                      step);
        }

        invariant(Connectivity_Update(&controller, &inputs, &outputs) == CONNECTIVITY_OK,
                  "state-machine update failed",
                  step);
        invariant((outputs.mode >= CONNECTIVITY_M1_INITIALISING) &&
                      (outputs.mode <= CONNECTIVITY_M4_FAULT),
                  "mode outside M1..M4",
                  step);
        invariant(outputs.active_source <= AUDIO_SOURCE_BLUETOOTH,
                  "invalid audio-source enum",
                  step);

        if ((outputs.mode == CONNECTIVITY_M3_IDLE) ||
            (outputs.mode == CONNECTIVITY_M4_FAULT) ||
            (outputs.mode == CONNECTIVITY_M1_INITIALISING)) {
            invariant(outputs.muted, "non-active mode was not muted", step);
        }
        if (outputs.mode == CONNECTIVITY_M2_ACTIVE) {
            invariant(outputs.active_source != AUDIO_SOURCE_NONE,
                      "M2 has no active source",
                      step);
            invariant(!outputs.muted, "M2 was muted", step);
        }
        if (outputs.active_source == AUDIO_SOURCE_BLUETOOTH) {
            invariant(controller.bluetooth_enabled && inputs.bluetooth_connected,
                      "Bluetooth selected without an enabled valid link",
                      step);
        }
        if (outputs.active_source == AUDIO_SOURCE_TELECOIL) {
            invariant(controller.telecoil_enabled && inputs.telecoil_valid,
                      "Telecoil selected without an enabled valid signal",
                      step);
        }
        if ((outputs.mode == CONNECTIVITY_M2_ACTIVE) && controller.bluetooth_enabled &&
            inputs.bluetooth_connected && controller.telecoil_enabled && inputs.telecoil_valid) {
            invariant(outputs.active_source == AUDIO_SOURCE_BLUETOOTH,
                      "Bluetooth priority invariant violated",
                      step);
        }
    }

    printf("FUZZ RESULT: %u deterministic state-machine updates, 0 invariant failures, OK\n",
           FUZZ_STEPS);
    return EXIT_SUCCESS;
}
