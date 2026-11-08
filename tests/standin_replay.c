/* standin_replay: the stand-in scripts' frames, replayed on the host (abacus #12095, after tally #12086 found expr082.txt
 * stale under unit 083). Each script in device/standin that sends FRAMECRC carries one header line:
 *   # replay: current                  every FRAMECRC is replayed here and must give the script's CRC
 *   # retired at <unit>: <why>         listed, not replayed (the script stays as its unit's board evidence)
 * A FRAMECRC script with neither fails, so each is decided once. A unit that moves a current script's frame updates the
 * script in the same commit or retires it there.
 *
 * The replay is the board's own sequence (device/standin/main/standin_main.c) through app.c: K<n> is app_key and U<n>
 * app_key_up, a quarter second apart (the scripts' sleep); SAVE keeps the calculator's image, WIPE drops it, and RESTART
 * boots again as the board does (ab_init, the image loaded if there is one, app_init, app_powered_on). FRAMECRC is the
 * CRC of rows 21-205 of the frame screen_render draws, as the board's; each `expect framecrc` reads the latest one. A
 * current script matches at least one frame and has a control: a "# CONTROL" line, then a framecrc expect before the next
 * send, which must differ. A retirement names its unit (digits) and a reason after the colon (abacus #12105).
 * X, BENCH and the waits and expects other than framecrc read the board and change nothing; a current script that sends
 * anything else can't be replayed and fails. `expect ver=` is ignored: the header carries the status. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "state.h"
#include "screen.h"
#include "ls027.h"

static ab_calc calc;
static app_state app;
static ls027_frame frame;
static uint32_t now;
static uint8_t image[65536];
static size_t image_len;

static void boot(void)
{
    ab_init(&calc);
    if (image_len && ab_state_load(&calc, image, image_len) != ABN_OK) ab_init(&calc);
    app_init(&app, &calc, now);
    app_powered_on(&app);
}

static unsigned long frame_crc(void)
{
    screen_ui ui;
    app_ui(&app, &ui);
    screen_render(&calc, &ui, &frame);
    return (unsigned long)ab_crc32((const uint8_t *)frame.px[21], (205 - 21 + 1) * LS027_ROW_BYTES);
}

/* one script: 0 when it passes (or is retired), else the number of failures */
static int replay(const char *path)
{
    FILE *fp = fopen(path, "r");
    if (!fp) {
        printf("standin_replay: %s: can't open\n", path);
        return 1;
    }
    char line[1024], status[1024] = "";
    int crcs = 0;
    while (fgets(line, sizeof line, fp)) {
        line[strcspn(line, "\r\n")] = 0;
        if (!strcmp(line, "send FRAMECRC")) crcs++;
        else if (!status[0] && (!strcmp(line, "# replay: current") || !strncmp(line, "# retired at ", 13)))
            snprintf(status, sizeof status, "%s", line + 2);
    }
    if (!crcs) {
        fclose(fp);
        return 0;
    }
    if (!status[0]) {
        printf("standin_replay: %s: FAIL: sends FRAMECRC with no header line (# replay: current, or # retired at <unit>: <why>)\n", path);
        fclose(fp);
        return 1;
    }
    if (strcmp(status, "replay: current")) {
        /* abacus #12105 G3: a retirement names its unit (digits) and says why after the colon */
        const char *u = status + 11, *d = u;
        while (*d >= '0' && *d <= '9') d++;
        const char *why = d[0] == ':' ? d + 1 : NULL;
        while (why && *why == ' ') why++;
        int retired_ok = d > u && why && *why;
        printf("standin_replay: %s: %s%s\n", path, retired_ok ? "" : "FAIL: a retirement needs a unit and a reason (# retired at <unit>: <why>): ", status);
        fclose(fp);
        return !retired_ok;
    }

    rewind(fp);
    now = 0;
    image_len = 0;
    boot();
    int n = 0, ok = 0, bad = 0, control = 0, framed = 0, frames = 0, controls = 0;
    unsigned long got = 0;
    while (fgets(line, sizeof line, fp)) {
        n++;
        line[strcspn(line, "\r\n")] = 0;
        if (!strncmp(line, "# CONTROL", 9)) control = 1;
        if (line[0] == '#' || !line[0] || !strncmp(line, "sleep", 5) || !strncmp(line, "wait", 4)) continue;
        if (!strncmp(line, "expect", 6)) {
            if (!strncmp(line, "expect framecrc: rows 21-205 ", 29)) {
                /* against the latest FRAMECRC, as talk.py reads the lines since the last send: the control after a
                   frame's own expect, with no send between, is checked against that frame */
                unsigned long want = strtoul(line + 29, NULL, 16);
                if (!framed) {
                    printf("standin_replay: %s:%d: FAIL: framecrc expected before any FRAMECRC\n", path, n);
                    bad++;
                } else if (control) {
                    if (want == got) {
                        printf("standin_replay: %s:%d: FAIL: the control's CRC %08lX is the frame's\n", path, n, want);
                        bad++;
                    }
                    control = 0;
                    controls++;
                } else if (frames++, want == got) ok++;
                else {
                    printf("standin_replay: %s:%d: FAIL: framecrc %08lX, the replay gives %08lX\n", path, n, want, got);
                    bad++;
                }
            }
            continue;
        }
        const char *s = line + 5;
        if (control) {                          /* abacus #12105 G2: a control is a framecrc expect before the next send */
            printf("standin_replay: %s:%d: FAIL: # CONTROL with no framecrc expect before the next send\n", path, n);
            bad++;
            control = 0;
        }
        if (strncmp(line, "send ", 5)) {
            printf("standin_replay: %s:%d: FAIL: can't replay \"%s\"\n", path, n, line);
            bad++;
        } else if (s[0] == 'K' && s[1] >= '0' && s[1] <= '9') {
            now += 250;
            app_key(&app, atoi(s + 1), now);
        } else if (s[0] == 'U' && s[1] >= '0' && s[1] <= '9') {
            now += 250;
            app_key_up(&app, atoi(s + 1), now);
        } else if (!strcmp(s, "SAVE")) {
            image_len = ab_state_save_rows(&calc, image, sizeof image);   /* (unit 090: the board's save holds the page
                                                   since 089, dev_image_save; the replay's didn't, and 090 draws it) */
        } else if (!strcmp(s, "WIPE")) {
            image_len = 0;
        } else if (!strcmp(s, "RESTART")) {
            now += 3000;
            boot();
        } else if (!strcmp(s, "FRAMECRC")) {
            got = frame_crc();
            framed = 1;
        } else if (strcmp(s, "X") && strcmp(s, "BENCH")) {
            printf("standin_replay: %s:%d: FAIL: can't replay \"%s\"\n", path, n, line);
            bad++;
        }
    }
    fclose(fp);
    if (control) {
        printf("standin_replay: %s: FAIL: # CONTROL with no framecrc expect after it\n", path);
        bad++;
    }
    if (!frames) {                              /* abacus #12105 G1: a current script matches at least one frame */
        printf("standin_replay: %s: FAIL: current, with no framecrc expect to match\n", path);
        bad++;
    }
    if (!controls) {                            /* G2: and ends with a control that must differ */
        printf("standin_replay: %s: FAIL: current, with no framecrc control (# CONTROL, then its expect)\n", path);
        bad++;
    }
    printf("standin_replay: %s: current, %d of %d frames match, %d control%s%s\n", path, ok, frames, controls,
           controls == 1 ? "" : "s", bad ? ": FAILED" : "");
    return bad;
}

int main(int argc, char **argv)
{
    int bad = 0;
    for (int i = 1; i < argc; i++) bad += replay(argv[i]);
    printf("standin_replay: %d script%s, %s\n", argc - 1, argc == 2 ? "" : "s", bad ? "FAILED" : "every current frame matches");
    return bad ? 1 : 0;
}
