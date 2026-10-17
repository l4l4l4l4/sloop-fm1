/* SPDX-License-Identifier: GPL-3.0-only */
/* Exercise the real synth and panel; no MIDI, audio or UI mocks in the changed path. */
#define UI_PAGES_HARNESS_ONLY
#include "ui_pages_test.c"

static uint32_t dub_engine(void)
{
    uint32_t e;
    for (e = 0; e < NENGINES; e++)
        if (!strcmp(ENGINES[e]->name, "DUB"))
            return e;
    return NENGINES;
}

/* Rising zero crossings on the dry mono voice make a held-note octave shift measurable. */
static uint32_t crossings(uint32_t blocks)
{
    int32_t out[CTL];
    int32_t prev = 0;
    uint32_t count = 0, b, i;
    for (b = 0; b < blocks; b++) {
        memset(out, 0, sizeof out);
        track_render(TSEL, out, CTL);
        for (i = 0; i < CTL; i++) {
            count += prev <= 0 && out[i] > 0;
            prev = out[i];
        }
    }
    return count;
}

static void boundary_pitch(uint32_t note, int16_t offset, int16_t bend, double hz)
{
    uint32_t got, want = (uint32_t)(hz * 3000u * CTL / FS + 0.5);
    apply_preset(0);
    frames(4);
    TSEL->p[P_LD_PIT] = TSEL->p[P_ED_PIT] = 0;
    TSEL->p[P_ATK] = 0;
    TSEL->p[P_SUS] = TSEL->p[P_E3] = 127;
    TSEL->p[P_E2] = 0;
    TSEL->p[P_E0] = offset;
    song.g[G_BEND] = 12;
    TSEL->bend = TSEL->bend_s = bend;
    trk_note_on(TSEL, note, 100);
    crossings(100);
    got = crossings(3000);
    check(got >= want * 97 / 100 && got <= want * 103 / 100,
          bend > 0 ? "high-key bend and negative live pitch combine without early saturation"
                   : "low-key bend and positive live pitch combine without early saturation");
    trk_note_off(TSEL, note);
    crossings(10000);
}

int main(int argc, char **argv)
{
    uint32_t e = dub_engine(), p, i, low, high;
    uint32_t expected = 440u * 1400u * CTL / FS;
    int16_t other[P_COUNT];
    outdir = argc > 1 ? argv[1] : "build/host";
    check(e < NENGINES, "DUB instrument is available without replacing existing engines");
    if (e == NENGINES)
        return 1;
    panel = PANEL_DEFAULT;
    layers_init();
    host_tracks_init();
    song.sel = 0;
    set_engine(e);
    frames(4);
    go_home();
    open_family(FAM_EDIT);
    knob_accel_off = 1;
    memcpy(other, trk[1].p, sizeof other);
    for (p = 0; p < ENGINES[e]->npresets; p++) {
        apply_preset(p);
        frames(4);
        TSEL->p[P_LD_PIT] = 0;
        TSEL->p[P_ED_PIT] = 0;
        TSEL->p[P_ATK] = 0;
        TSEL->p[P_SUS] = 127;
        TSEL->p[P_E0] = 0;
        trk_note_on(TSEL, 69, 100);
        low = crossings(1400);
        encs[panel.enc[EN_K1]] = 12;
        frame();
        high = crossings(1400);
        check(low > expected * 98 / 100 && low < expected * 102 / 100 &&
              high > low * 19 / 10 && high < low * 21 / 10,
              "PITCH knob raises a held voice by an octave without retriggering");
        encs[panel.enc[EN_K1]] = -24;
        frame();
        high = crossings(1400);
        check(high > low * 48 / 100 && high < low * 52 / 100,
              "PITCH knob lowers the same held voice below the played note");
        encs[panel.enc[EN_K1]] = 12;
        encs[panel.enc[EN_K2]] = 1;
        encs[panel.enc[EN_K3]] = 32;
        frame();
        {
            uint32_t lo = UINT32_MAX, hi = 0, j;
            uint32_t window = 440u * 100u * CTL / FS;
            for (j = 0; j < 64; j++) {
                uint32_t c = crossings(100);
                if (c < lo) lo = c;
                if (c > hi) hi = c;
            }
            check(lo < window * 80 / 100 && hi > window * 120 / 100,
                  "performance DEPTH knob makes the held pitch sweep above and below the key");
        }
        trk_note_off(TSEL, 69);
        crossings(10000);
        {
            uint32_t active = 0;
            for (i = 0; i < NVOICE; i++)
                active += TSEL->v[i].active;
            check(!active, "released siren leaves no hanging dry voice");
        }
    }
    apply_preset(0);
    frames(4);
    check(!memcmp(other, trk[1].p, sizeof other), "DUB controls leave the other synth track unchanged");
    boundary_pitch(120, -24, 8191, 4186.009);
    boundary_pitch(9, 24, -8192, 27.5);
    printf("dub siren: %d failures\n", fails);
    return fails != 0;
}
