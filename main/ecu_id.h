#pragma once

/* ECU identification probe: who made this ECU, and does it speak UDS?
 *
 * The ZS125 ECU is labelled FAI / New Means 85400-1614 but its housing
 * matches a Bosch MSE6.0. Bosch ECUs give themselves away in their
 * identification data (supplier "BOSCH", hardware numbers like
 * 0 261 S0x xxx, software numbers starting 1037...), so this asks for all
 * of it and prints the raw replies:
 *
 *   - OBD Mode 09 on the functional ID (VIN, calibration ID, CVN, ECU name);
 *   - UDS ReadDataByIdentifier (22 F1xx) and KWP2000 readEcuIdentification
 *     (1A xx) on the physical ID, plus a few on the functional ID;
 *   - engine stopped only: extended diagnostic session (10 03), then
 *     securityAccess "request seed" (27 01/03/05/11) to see which levels
 *     exist and how long the seed is, then back to the default session.
 *
 * Read-only by design: no key is ever sent (wrong keys can lock the ECU
 * out), no programming session (10 02), no writes, no routines, no reset.
 *
 * Pure C, no ESP-IDF: the caller sends each step and hands the reply to
 * ecu_id_report(), which formats it as text lines. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ECU_ID_REQ_MAX 4

typedef enum {
    ECU_ID_FUNCTIONAL, /* 0x7DF on this bike */
    ECU_ID_PHYSICAL,   /* 0x7E0: the engine ECU's own request ID */
} ecu_id_target_t;

typedef struct {
    ecu_id_target_t target;
    bool engine_off_only; /* changes the diagnostic session */
    uint8_t len;
    uint8_t req[ECU_ID_REQ_MAX];
    const char *label;
} ecu_id_step_t;

typedef enum {
    ECU_ID_OK,
    ECU_ID_NEGATIVE, /* reply holds 7F <service> <NRC> */
    ECU_ID_TIMEOUT,
    ECU_ID_SKIPPED,  /* engine running */
} ecu_id_status_t;

typedef void (*ecu_id_line_fn)(const char *line, void *ctx);

/* The probe, in the order it should run. */
size_t ecu_id_steps(const ecu_id_step_t **steps);

/* Short meaning of a negative response code, e.g. "serviceNotSupported". */
const char *ecu_id_nrc_name(uint8_t nrc);

/* One step's result as text lines, each under 120 characters:
 *   ECUID P 22F18A supplier: OK 9 bytes
 *   ECUID   62 F1 8A 42 4F 53 43 48 00  |b..BOSCH.|
 *   ECUID F 0902 VIN: NEGATIVE 12 subFunctionNotSupported
 * P / F = physical / functional request. Long replies wrap at 16 bytes. */
void ecu_id_report(const ecu_id_step_t *step, ecu_id_status_t status, const uint8_t *reply,
                   size_t reply_len, ecu_id_line_fn out, void *ctx);
