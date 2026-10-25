/* Host test for main/ecu_id.c:
 *   gcc -std=c11 -Wall -Wextra -Werror -I main test/host/test_ecu_id.c main/ecu_id.c
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "ecu_id.h"

static char s_lines[8][128];
static int s_count;

static void collect(const char *line, void *ctx)
{
    (void)ctx;
    assert(s_count < 8);
    assert(strlen(line) < 120);
    strcpy(s_lines[s_count++], line);
}

int main(void)
{
    const ecu_id_step_t *steps;
    size_t n = ecu_id_steps(&steps);
    assert(n > 10);

    /* Read-only: never a key (27 even), programming session (10 02),
     * write (2E), routine (31), reset (11) or download (34). The session
     * steps run with the engine stopped only and end in the default
     * session. */
    int last_session = -1;
    for (size_t i = 0; i < n; ++i) {
        const ecu_id_step_t *s = &steps[i];
        assert(s->len >= 1 && s->len <= ECU_ID_REQ_MAX);
        assert(s->label != NULL);
        uint8_t svc = s->req[0];
        assert(svc != 0x2E && svc != 0x31 && svc != 0x11 && svc != 0x34 && svc != 0x3D);
        if (svc == 0x27) {
            assert((s->req[1] & 1) == 1);
            assert(s->engine_off_only);
        }
        if (svc == 0x10) {
            assert(s->req[1] != 0x02);
            assert(s->engine_off_only);
            last_session = (int)i;
        }
    }
    assert(last_session >= 0 && steps[last_session].req[1] == 0x01);
    for (size_t i = (size_t)last_session + 1; i < n; ++i) assert(steps[i].req[0] != 0x27);

    ecu_id_step_t supplier = { ECU_ID_PHYSICAL, false, 3, { 0x22, 0xF1, 0x8A }, "system supplier" };
    const uint8_t bosch[] = { 0x62, 0xF1, 0x8A, 'B', 'O', 'S', 'C', 'H', 0x00 };
    s_count = 0;
    ecu_id_report(&supplier, ECU_ID_OK, bosch, sizeof(bosch), collect, NULL);
    assert(s_count == 2);
    assert(!strcmp(s_lines[0], "ECUID P 22F18A system supplier: OK 9 bytes"));
    assert(strstr(s_lines[1], " 62 F1 8A 42 4F 53 43 48 00") != NULL);
    assert(strstr(s_lines[1], "|b..BOSCH.|") != NULL);

    /* 20 bytes wrap onto two dump lines. */
    uint8_t vin[20];
    vin[0] = 0x49;
    vin[1] = 0x02;
    vin[2] = 0x01;
    memcpy(vin + 3, "LZSJCNP0123456789", 17);
    ecu_id_step_t vin_step = { ECU_ID_FUNCTIONAL, false, 2, { 0x09, 0x02 }, "VIN" };
    s_count = 0;
    ecu_id_report(&vin_step, ECU_ID_OK, vin, sizeof(vin), collect, NULL);
    assert(s_count == 3);
    assert(!strncmp(s_lines[0], "ECUID F 0902 VIN: OK 20 bytes", 29));
    assert(strstr(s_lines[1], "|I..LZSJCNP012345|") != NULL);
    assert(strstr(s_lines[2], "|6789|") != NULL);

    const uint8_t neg[] = { 0x7F, 0x22, 0x31 };
    s_count = 0;
    ecu_id_report(&supplier, ECU_ID_NEGATIVE, neg, sizeof(neg), collect, NULL);
    assert(s_count == 1);
    assert(!strcmp(s_lines[0], "ECUID P 22F18A system supplier: NEGATIVE 31 requestOutOfRange"));

    s_count = 0;
    ecu_id_report(&supplier, ECU_ID_TIMEOUT, NULL, 0, collect, NULL);
    assert(!strcmp(s_lines[0], "ECUID P 22F18A system supplier: NO REPLY"));
    s_count = 0;
    ecu_id_report(&steps[last_session], ECU_ID_SKIPPED, NULL, 0, collect, NULL);
    assert(strstr(s_lines[0], "SKIPPED engine running") != NULL);

    assert(!strcmp(ecu_id_nrc_name(0x7F), "serviceNotSupportedInActiveSession"));
    assert(!strcmp(ecu_id_nrc_name(0xEE), "unknown"));

    printf("ecu_id: all tests passed\n");
    return 0;
}
