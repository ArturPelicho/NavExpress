#include "ecu_id.h"

#include <stdio.h>

#define F ECU_ID_FUNCTIONAL
#define P ECU_ID_PHYSICAL

static const ecu_id_step_t s_steps[] = {
    /* OBD Mode 09: answered by every OBD ECU that implements it. */
    { F, false, 2, { 0x09, 0x00 }, "Mode 09 supported" },
    { F, false, 2, { 0x09, 0x02 }, "VIN" },
    { F, false, 2, { 0x09, 0x04 }, "calibration ID" },
    { F, false, 2, { 0x09, 0x06 }, "CVN" },
    { F, false, 2, { 0x09, 0x0A }, "ECU name" },

    /* Does the ECU answer on its physical ID at all? */
    { P, false, 2, { 0x3E, 0x00 }, "tester present" },
    { P, false, 3, { 0x22, 0xF1, 0x86 }, "active session" },

    /* UDS identification (ISO 14229-1 annex C). */
    { P, false, 3, { 0x22, 0xF1, 0x87 }, "spare part number" },
    { P, false, 3, { 0x22, 0xF1, 0x88 }, "ECU software number" },
    { P, false, 3, { 0x22, 0xF1, 0x89 }, "ECU software version" },
    { P, false, 3, { 0x22, 0xF1, 0x8A }, "system supplier" },
    { P, false, 3, { 0x22, 0xF1, 0x8B }, "manufacturing date" },
    { P, false, 3, { 0x22, 0xF1, 0x8C }, "ECU serial number" },
    { P, false, 3, { 0x22, 0xF1, 0x90 }, "VIN" },
    { P, false, 3, { 0x22, 0xF1, 0x91 }, "ECU hardware number" },
    { P, false, 3, { 0x22, 0xF1, 0x92 }, "supplier hardware number" },
    { P, false, 3, { 0x22, 0xF1, 0x93 }, "supplier hardware version" },
    { P, false, 3, { 0x22, 0xF1, 0x94 }, "supplier software number" },
    { P, false, 3, { 0x22, 0xF1, 0x95 }, "supplier software version" },
    { P, false, 3, { 0x22, 0xF1, 0x97 }, "system name" },
    { P, false, 3, { 0x22, 0xF1, 0x80 }, "boot software ID" },
    { P, false, 3, { 0x22, 0xF1, 0x81 }, "application software ID" },
    { P, false, 3, { 0x22, 0xF1, 0x82 }, "application data ID" },

    /* KWP2000 readEcuIdentification: older Bosch motorcycle ECUs. */
    { P, false, 2, { 0x1A, 0x80 }, "KWP ECU identification" },
    { P, false, 2, { 0x1A, 0x86 }, "KWP manufacturer ID" },
    { P, false, 2, { 0x1A, 0x87 }, "KWP spare part number" },
    { P, false, 2, { 0x1A, 0x8A }, "KWP supplier" },
    { P, false, 2, { 0x1A, 0x90 }, "KWP VIN" },
    { P, false, 2, { 0x1A, 0x91 }, "KWP hardware number" },
    { P, false, 2, { 0x1A, 0x92 }, "KWP supplier hardware number" },
    { P, false, 2, { 0x1A, 0x94 }, "KWP supplier software number" },
    { P, false, 2, { 0x1A, 0x9B }, "KWP Bosch calibration ID" },

    /* Same key identifiers on the functional ID, in case the physical ID
     * is not 0x7E0. */
    { F, false, 3, { 0x22, 0xF1, 0x8A }, "system supplier" },
    { F, false, 3, { 0x22, 0xF1, 0x91 }, "ECU hardware number" },
    { F, false, 3, { 0x22, 0xF1, 0x94 }, "supplier software number" },

    /* Engine stopped only. Extended session, then which security levels
     * hand out a seed. No key is sent; the session drops back to default
     * at the end (and by itself after a few seconds anyway). */
    { P, true, 2, { 0x10, 0x03 }, "extended session" },
    { P, true, 3, { 0x22, 0xF1, 0x86 }, "active session" },
    { P, true, 2, { 0x27, 0x01 }, "seed level 01" },
    { P, true, 2, { 0x27, 0x03 }, "seed level 03" },
    { P, true, 2, { 0x27, 0x05 }, "seed level 05" },
    { P, true, 2, { 0x27, 0x11 }, "seed level 11" },
    { P, true, 2, { 0x10, 0x01 }, "default session" },
};

/* Deeper read-only probe (ECU PROBE). All engine-off. */
static const ecu_id_step_t s_probe_steps[] = {
    { P, true, 2, { 0x10, 0x03 }, "extended session" },

    /* Read the seed three times in one session: if it is always 56 78 it is
     * fixed (and so is the key). Run this command again after a key cycle to
     * confirm it does not change between sessions either. */
    { P, true, 2, { 0x27, 0x01 }, "seed level 01 read 1" },
    { P, true, 2, { 0x27, 0x01 }, "seed level 01 read 2" },
    { P, true, 2, { 0x27, 0x01 }, "seed level 01 read 3" },

    /* ReadMemoryByAddress (service 23). The second byte is the
     * addressAndLengthFormatIdentifier: low nibble = address bytes, high
     * nibble = size bytes. Try the common layouts, reading 16 bytes from
     * address 0. NRC 33 = needs the key; 31 = wrong address but the service
     * is open; 11 = not supported. 7 bytes max (single frame). */
    { P, true, 5, { 0x23, 0x12, 0x00, 0x00, 0x10 }, "read mem 23 (2-byte addr)" },
    { P, true, 7, { 0x23, 0x14, 0x00, 0x00, 0x00, 0x00, 0x10 }, "read mem 23 (4-byte addr)" },
    { P, true, 6, { 0x23, 0x24, 0x00, 0x00, 0x00, 0x10 }, "read mem 23 (3-byte addr)" },

    /* RequestUpload (service 35): asks the ECU to send memory out (read
     * direction, no write). dataFormatIdentifier 00, then the same
     * addressAndLengthFormatIdentifier, address and size. A positive 74
     * reply means a dump is allowed without a key. */
    { P, true, 6, { 0x35, 0x00, 0x11, 0x00, 0x00, 0x10 }, "request upload 35 (1-byte)" },
    { P, true, 7, { 0x35, 0x00, 0x12, 0x00, 0x00, 0x00, 0x10 }, "request upload 35 (2-byte)" },

    { P, true, 2, { 0x10, 0x01 }, "default session" },
};

#undef F
#undef P

size_t ecu_id_steps(const ecu_id_step_t **steps)
{
    *steps = s_steps;
    return sizeof(s_steps) / sizeof(s_steps[0]);
}

size_t ecu_probe_steps(const ecu_id_step_t **steps)
{
    *steps = s_probe_steps;
    return sizeof(s_probe_steps) / sizeof(s_probe_steps[0]);
}

const char *ecu_id_nrc_name(uint8_t nrc)
{
    switch (nrc) {
    case 0x10: return "generalReject";
    case 0x11: return "serviceNotSupported";
    case 0x12: return "subFunctionNotSupported";
    case 0x13: return "incorrectMessageLength";
    case 0x14: return "responseTooLong";
    case 0x21: return "busyRepeatRequest";
    case 0x22: return "conditionsNotCorrect";
    case 0x24: return "requestSequenceError";
    case 0x31: return "requestOutOfRange";
    case 0x33: return "securityAccessDenied";
    case 0x35: return "invalidKey";
    case 0x36: return "exceededNumberOfAttempts";
    case 0x37: return "requiredTimeDelayNotExpired";
    case 0x7E: return "subFunctionNotSupportedInActiveSession";
    case 0x7F: return "serviceNotSupportedInActiveSession";
    default: return "unknown";
    }
}

#define BYTES_PER_LINE 16

void ecu_id_report(const ecu_id_step_t *step, ecu_id_status_t status, const uint8_t *reply,
                   size_t reply_len, ecu_id_line_fn out, void *ctx)
{
    char line[128];
    char req[2 * ECU_ID_REQ_MAX + 1];
    int used = 0;
    for (uint8_t i = 0; i < step->len && i < ECU_ID_REQ_MAX; ++i) {
        used += snprintf(req + used, sizeof(req) - (size_t)used, "%02X", step->req[i]);
    }
    const char *head = step->target == ECU_ID_PHYSICAL ? "P" : "F";

    switch (status) {
    case ECU_ID_SKIPPED:
        snprintf(line, sizeof(line), "ECUID %s %s %s: SKIPPED engine running", head, req,
                 step->label);
        out(line, ctx);
        return;
    case ECU_ID_TIMEOUT:
        snprintf(line, sizeof(line), "ECUID %s %s %s: NO REPLY", head, req, step->label);
        out(line, ctx);
        return;
    case ECU_ID_NEGATIVE: {
        uint8_t nrc = reply_len >= 3 ? reply[2] : 0;
        snprintf(line, sizeof(line), "ECUID %s %s %s: NEGATIVE %02X %s", head, req, step->label,
                 nrc, ecu_id_nrc_name(nrc));
        out(line, ctx);
        return;
    }
    case ECU_ID_OK:
    default:
        break;
    }

    snprintf(line, sizeof(line), "ECUID %s %s %s: OK %u bytes", head, req, step->label,
             (unsigned)reply_len);
    out(line, ctx);
    for (size_t start = 0; start < reply_len; start += BYTES_PER_LINE) {
        size_t n = reply_len - start < BYTES_PER_LINE ? reply_len - start : BYTES_PER_LINE;
        used = snprintf(line, sizeof(line), "ECUID  ");
        for (size_t i = 0; i < n; ++i) {
            used += snprintf(line + used, sizeof(line) - (size_t)used, " %02X", reply[start + i]);
        }
        used += snprintf(line + used, sizeof(line) - (size_t)used, "%*s  |",
                         (int)(3 * (BYTES_PER_LINE - n)), "");
        for (size_t i = 0; i < n; ++i) {
            uint8_t c = reply[start + i];
            line[used++] = c >= 0x20 && c < 0x7F ? (char)c : '.';
        }
        line[used++] = '|';
        line[used] = '\0';
        out(line, ctx);
    }
}
