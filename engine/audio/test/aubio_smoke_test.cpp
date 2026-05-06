#include <cstdio>
#include <aubio/aubio.h>

int main()
{
    const uint_t win_s = 1024;
    const uint_t hop_s = 512;
    const uint_t sr = 44100;

    aubio_pvoc_t *pvoc = new_aubio_pvoc(win_s, hop_s);
    printf("pvoc: %s\n", pvoc ? "OK" : "FAIL");

    aubio_tempo_t *tempo = new_aubio_tempo("default", win_s, hop_s, sr);
    printf("tempo: %s\n", tempo ? "OK" : "FAIL");

    aubio_pitch_t *pitch = new_aubio_pitch("yinfft", win_s, hop_s, sr);
    printf("pitch: %s\n", pitch ? "OK" : "FAIL");

    const char *onset_methods[] = {"energy", "hfc", "complex", "specflux", "phase"};
    for (int i = 0; i < 5; i++) {
        aubio_onset_t *onset = new_aubio_onset(onset_methods[i], win_s, hop_s, sr);
        printf("onset(%s): %s\n", onset_methods[i], onset ? "OK" : "FAIL");
        if (onset) del_aubio_onset(onset);
    }

    aubio_mfcc_t *mfcc = new_aubio_mfcc(win_s, 40, 13, sr);
    printf("mfcc: %s\n", mfcc ? "OK" : "FAIL");

    aubio_filterbank_t *fb = new_aubio_filterbank(40, win_s);
    if (fb) aubio_filterbank_set_mel_coeffs_slaney(fb, sr);
    printf("filterbank: %s\n", fb ? "OK" : "FAIL");

    const char *desc_methods[] = {"centroid", "spread", "rolloff", "specflux", "hfc"};
    for (int i = 0; i < 5; i++) {
        aubio_specdesc_t *desc = new_aubio_specdesc(desc_methods[i], win_s);
        printf("specdesc(%s): %s\n", desc_methods[i], desc ? "OK" : "FAIL");
        if (desc) del_aubio_specdesc(desc);
    }

    aubio_tss_t *tss = new_aubio_tss(win_s, hop_s);
    printf("tss: %s\n", tss ? "OK" : "FAIL");

    aubio_notes_t *notes = new_aubio_notes("default", win_s, hop_s, sr);
    printf("notes: %s\n", notes ? "OK" : "FAIL");

    if (pvoc) del_aubio_pvoc(pvoc);
    if (tempo) del_aubio_tempo(tempo);
    if (pitch) del_aubio_pitch(pitch);
    if (mfcc) del_aubio_mfcc(mfcc);
    if (fb) del_aubio_filterbank(fb);
    if (tss) del_aubio_tss(tss);
    if (notes) del_aubio_notes(notes);

    printf("\nAll aubio objects created and destroyed successfully!\n");
    return 0;
}
