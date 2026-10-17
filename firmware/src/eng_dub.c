/* SPDX-License-Identifier: GPL-3.0-only */
/* DUB: one oscillator, the track's pitch LFO and pitch envelope, and its normal FX sends.
 * An original performance instrument, not a calibrated model of an analogue siren or delay. */
static const char *const N_DUB_WAVE[] = {"SIN", "SQR", "TRI"};

/* Integrated polyBLEP at a triangle corner (polyBLAMP). The slope changes by eight
 * full-scale units per cycle: dt * 4/3 * (1 - distance/dt)^3, in Q15. */
static inline int32_t dub_blamp(uint32_t ph, uint32_t inc)
{
    uint32_t dist = ph < 0x80000000u ? ph : 0u - ph, d = inc >> 15;
    int32_t x;
    if (!d || dist >= inc)
        return 0;
    x = clamp(32768 - (int32_t)(dist / d), 0, 32768);
    return mulq15(mulq15(mulq15(x, x), x), (int32_t)d / 3);
}

static void dub_note_on(track_t *t, voice_t *v)
{
    (void)t;
    if (!v->env && !v->env_out) {
        v->ph[0] = 0;
        v->s[0] = v->s[1] = 0;                       /* per-voice tone filter; keep it on a retrigger */
    }
}

static void dub_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t i, ph = v->ph[0], inc = m->inc, wave = (uint32_t)p[P_E1];
    int32_t ic1 = v->s[0], ic2 = v->s[1];
    int32_t drv = p[P_E2], drive = 32768 + drv * 768, wet = drv * 258;
    tsvf_t tone;
    tsvf_coef(&tone, (p[P_E3] << 8) + m->cutoff, 0);
    for (i = 0; i < n; i++) {
        int32_t s;
        if (wave == 1u)
            s = osc_pulse(ph, inc, 0x80000000u);
        else if (wave == 2u)
            s = osc_tri(ph) + dub_blamp(ph, inc) - dub_blamp(ph + 0x80000000u, inc);
        else
            s = osc_sine(ph);
        ph += inc;
        if (drv)
            s += mulq15(softclip(((s >> 2) * (drive >> 2)) >> 11) - s, wet);
        s = tsvf_lp(&tone, s >> 1, &ic1, &ic2) * 2;
        out[i] += voice_amp(s, m, i);
    }
    v->ph[0] = ph;
    v->s[0] = ic1;
    v->s[1] = ic2;
}

/* {live pitch, wave, drive, tone, unused x4}; one playable siren, released
 * through the shared ADSR. Wave and modulation remain controls of this sound. */
static const preset_t DUB_PRESETS[] = {
    {"DUB SIREN", {0, 0, 12, 112, 0, 0, 0, 0}, {2, 64, 127, 46}, 0, 1, FX(0, 0, 64, 10),
     XP(P_LRATE + 1, 50, P_LD_PIT + 1, 24, P_LWAVE + 1, 0, P_VOICE + 1, V_MONO)},
};

static const engine_t ENG_DUB = {
    .name = "DUB",
    .page_title = {"DUB TONE", 0},
    .edit = {
        {"PITCH", F_SEMI, -24, 24, 0, 0, 0},
        {"WAVE", F_ENUM, 0, NELEM(N_DUB_WAVE) - 1, 0, N_DUB_WAVE, 0},
        {"DRIVE", F_PCT, 0, 127, 12, 0, 0},
        {"TONE", F_CUTOFF, 0, 127, 112, 0, 0},
        {"--", F_INT, 0, 0, 0, 0, 0},
        {"--", F_INT, 0, 0, 0, 0, 0},
        {"--", F_INT, 0, 0, 0, 0, 0},
        {"--", F_INT, 0, 0, 0, 0, 0},
    },
    .presets = DUB_PRESETS,
    .npresets = NELEM(DUB_PRESETS),
    .fil_page = -1,
    .note_on = dub_note_on,
    .render = dub_render,
    .color = 0x07F5,
    .macro = {P_E0, P_LRATE, P_LD_PIT, P_DLY},
    .poly = 1,
};
