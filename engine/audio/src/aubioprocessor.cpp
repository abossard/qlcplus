#include "aubioprocessor.h"

#include <algorithm>
#include <cmath>
#include <cstring>

static const char *kOnsetMethods[9] = {
    "energy", "hfc", "complex", "phase", "wphase",
    "specdiff", "kl", "mkl", "specflux"
};

namespace {

// Validate a window-type string. aubio_pvoc_set_window accepts a fixed list;
// fall back to "default" so an XML/QML typo cannot brick the pvoc.
QByteArray sanitizedWindowType(const QString &name)
{
    static const QStringList kAllowed = {
        QStringLiteral("default"),    QStringLiteral("rectangle"),
        QStringLiteral("hamming"),    QStringLiteral("hanning"),
        QStringLiteral("hanningz"),   QStringLiteral("blackman"),
        QStringLiteral("blackman_harris"), QStringLiteral("gaussian"),
        QStringLiteral("welch"),      QStringLiteral("parzen")
    };
    if (kAllowed.contains(name))
        return name.toUtf8();
    return QByteArrayLiteral("default");
}

bool isHtkMelScale(const QString &name)
{
    return name.compare(QStringLiteral("htk"), Qt::CaseInsensitive) == 0;
}

void applyOnsetParams(aubio_onset_t *onset, const AubioConfig &cfg)
{
    if (!onset) return;
    aubio_onset_set_threshold(onset, cfg.onsetThreshold);
    aubio_onset_set_minioi_ms(onset, cfg.onsetMinIntervalMs);
    aubio_onset_set_silence(onset, cfg.onsetSilenceDb);
    if (cfg.onsetDelayMs > 0.0)
        aubio_onset_set_delay_ms(onset, cfg.onsetDelayMs);
    aubio_onset_set_awhitening(onset, cfg.onsetAdaptiveWhitening ? 1u : 0u);
    aubio_onset_set_compression(onset, smpl_t(cfg.onsetCompressionLambda));
}

} // namespace

AubioProcessor::AubioProcessor() {}

AubioProcessor::~AubioProcessor()
{
    release();
}

void AubioProcessor::initialize(uint32_t sampleRate)
{
    release();
    m_sampleRate = sampleRate;
    m_totalSamplesProcessed = 0;

    const uint_t win = windowSize();
    const uint_t hop = hopSize();

    m_pvoc = new_aubio_pvoc(win, hop);
    if (m_pvoc)
        aubio_pvoc_set_window(m_pvoc, sanitizedWindowType(m_config.windowType).constData());

    const QByteArray tempoMethod = m_config.tempoMethod.toUtf8();
    m_tempo = new_aubio_tempo(tempoMethod.constData(), win, hop, sampleRate);
    if (m_tempo)
    {
        aubio_tempo_set_tatum_signature(m_tempo, m_config.tatumSubdivision);
        aubio_tempo_set_silence(m_tempo, m_config.tempoSilenceDb);
        aubio_tempo_set_threshold(m_tempo, m_config.tempoThreshold);
        aubio_tempo_set_delay_ms(m_tempo, smpl_t(m_config.tempoDelayMs));
    }

    const QByteArray pitchMethod = m_config.pitchMethod.toUtf8();
    m_pitch = new_aubio_pitch(pitchMethod.constData(), win, hop, sampleRate);
    if (m_pitch)
    {
        // Always Hz: pitch unit is a display concern handled in QML, never
        // changed at the aubio layer.
        aubio_pitch_set_unit(m_pitch, "Hz");
        aubio_pitch_set_silence(m_pitch, m_config.pitchSilenceDb);
        aubio_pitch_set_tolerance(m_pitch, m_config.pitchTolerance);
    }

    m_notes = new_aubio_notes("default", win, hop, sampleRate);
    if (m_notes)
    {
        aubio_notes_set_silence(m_notes, smpl_t(m_config.noteSilenceDb));
        aubio_notes_set_minioi_ms(m_notes, smpl_t(m_config.noteMinIntervalMs));
        aubio_notes_set_release_drop(m_notes, smpl_t(m_config.noteReleaseDropDb));
    }

    for (int i = 0; i < kOnsetMethodCount; i++)
    {
        if (!m_config.onsetMethodEnabled[i])
            continue;
        m_onsets[i] = new_aubio_onset(kOnsetMethods[i], win, hop, sampleRate);
        applyOnsetParams(m_onsets[i], m_config);
    }

    m_mfcc = new_aubio_mfcc(win, AUBIO_MEL_BANDS, AUBIO_MFCC_COEFFS, sampleRate);
    if (m_mfcc)
    {
        aubio_mfcc_set_power(m_mfcc, smpl_t(m_config.mfccPower));
        aubio_mfcc_set_scale(m_mfcc, smpl_t(m_config.mfccScale));
    }

    m_filterbank = new_aubio_filterbank(AUBIO_MEL_BANDS, win);
    if (m_filterbank)
    {
        // norm must be set BEFORE set_mel_coeffs_* (it's read during coeff build).
        aubio_filterbank_set_norm(m_filterbank, smpl_t(m_config.filterbankNorm));
        if (isHtkMelScale(m_config.melScale))
            aubio_filterbank_set_mel_coeffs_htk(m_filterbank, sampleRate, 0.0, double(sampleRate) * 0.5);
        else
            aubio_filterbank_set_mel_coeffs_slaney(m_filterbank, sampleRate);
        aubio_filterbank_set_power(m_filterbank, smpl_t(m_config.filterbankPower));
    }

    m_descCentroid = new_aubio_specdesc("centroid", win);
    m_descSpread = new_aubio_specdesc("spread", win);
    m_descRolloff = new_aubio_specdesc("rolloff", win);
    m_descFlux = new_aubio_specdesc("specflux", win);
    m_descHfc = new_aubio_specdesc("hfc", win);

    m_tss = new_aubio_tss(win, hop);
    if (m_tss)
    {
        aubio_tss_set_alpha(m_tss, m_config.tssAlpha);
        aubio_tss_set_beta(m_tss, m_config.tssBeta);
        aubio_tss_set_threshold(m_tss, m_config.tssThreshold);
    }

    m_hopBuffer = new_fvec(hop);
    m_fftGrain = new_cvec(win);
    m_onsetOut = new_fvec(1);
    m_tempoOut = new_fvec(2);
    m_pitchOut = new_fvec(1);
    m_notesOut = new_fvec(3);
    m_melOut = new_fvec(AUBIO_MEL_BANDS);
    m_mfccOut = new_fvec(AUBIO_MFCC_COEFFS);
    m_descOut = new_fvec(1);
    m_transGrain = new_cvec(win);
    m_steadGrain = new_cvec(win);

    m_initialized = true;
}

void AubioProcessor::release()
{
    if (m_pvoc) { del_aubio_pvoc(m_pvoc); m_pvoc = nullptr; }
    if (m_tempo) { del_aubio_tempo(m_tempo); m_tempo = nullptr; }
    if (m_pitch) { del_aubio_pitch(m_pitch); m_pitch = nullptr; }
    if (m_notes) { del_aubio_notes(m_notes); m_notes = nullptr; }
    if (m_mfcc) { del_aubio_mfcc(m_mfcc); m_mfcc = nullptr; }
    if (m_filterbank) { del_aubio_filterbank(m_filterbank); m_filterbank = nullptr; }
    if (m_tss) { del_aubio_tss(m_tss); m_tss = nullptr; }
    for (int i = 0; i < kOnsetMethodCount; i++)
        if (m_onsets[i]) { del_aubio_onset(m_onsets[i]); m_onsets[i] = nullptr; }
    if (m_descCentroid) { del_aubio_specdesc(m_descCentroid); m_descCentroid = nullptr; }
    if (m_descSpread) { del_aubio_specdesc(m_descSpread); m_descSpread = nullptr; }
    if (m_descRolloff) { del_aubio_specdesc(m_descRolloff); m_descRolloff = nullptr; }
    if (m_descFlux) { del_aubio_specdesc(m_descFlux); m_descFlux = nullptr; }
    if (m_descHfc) { del_aubio_specdesc(m_descHfc); m_descHfc = nullptr; }

    if (m_hopBuffer) { del_fvec(m_hopBuffer); m_hopBuffer = nullptr; }
    if (m_fftGrain) { del_cvec(m_fftGrain); m_fftGrain = nullptr; }
    if (m_onsetOut) { del_fvec(m_onsetOut); m_onsetOut = nullptr; }
    if (m_tempoOut) { del_fvec(m_tempoOut); m_tempoOut = nullptr; }
    if (m_pitchOut) { del_fvec(m_pitchOut); m_pitchOut = nullptr; }
    if (m_notesOut) { del_fvec(m_notesOut); m_notesOut = nullptr; }
    if (m_melOut) { del_fvec(m_melOut); m_melOut = nullptr; }
    if (m_mfccOut) { del_fvec(m_mfccOut); m_mfccOut = nullptr; }
    if (m_descOut) { del_fvec(m_descOut); m_descOut = nullptr; }
    if (m_transGrain) { del_cvec(m_transGrain); m_transGrain = nullptr; }
    if (m_steadGrain) { del_cvec(m_steadGrain); m_steadGrain = nullptr; }

    m_initialized = false;
}

void AubioProcessor::setPendingConfig(const AubioConfig &cfg)
{
    QMutexLocker locker(&m_configMutex);
    m_pendingConfig = cfg;
    m_hasPendingConfig = true;
}

bool AubioProcessor::needsFullRebuild(const AubioConfig &o, const AubioConfig &n) const
{
    // Algorithm-string changes, filterbank norm changes, and analysis window /
    // mel scale changes require destroying and recreating aubio objects.
    // Filterbank norm must be set BEFORE set_mel_coeffs_*, the window type is
    // baked into the pvoc internals at construction in some aubio paths, and
    // mel coefficients can't be swapped between slaney/htk on the fly.
    // Onset method enable/disable is intentionally NOT here — see
    // applyParamUpdates() for the targeted create/destroy of individual onset
    // detectors.
    if (o.pitchMethod != n.pitchMethod) return true;
    if (o.tempoMethod != n.tempoMethod) return true;
    if (o.filterbankNorm != n.filterbankNorm) return true;
    if (o.windowType != n.windowType) return true;
    if (o.melScale != n.melScale) return true;
    return false;
}

void AubioProcessor::applyParamUpdates(const AubioConfig &oldCfg, const AubioConfig &cfg)
{
    if (m_filterbank)
        aubio_filterbank_set_power(m_filterbank, smpl_t(cfg.filterbankPower));

    // Targeted onset enable/disable: only create/destroy the detectors whose
    // enabled flag actually changed. Avoids tearing down the rest of the
    // pipeline when toggling a single method.
    for (int i = 0; i < kOnsetMethodCount; i++)
    {
        if (oldCfg.onsetMethodEnabled[i] != cfg.onsetMethodEnabled[i])
        {
            if (m_onsets[i])
            {
                del_aubio_onset(m_onsets[i]);
                m_onsets[i] = nullptr;
            }
            if (cfg.onsetMethodEnabled[i])
            {
                m_onsets[i] = new_aubio_onset(kOnsetMethods[i],
                                              windowSize(), hopSize(), m_sampleRate);
            }
        }
    }

    for (int i = 0; i < kOnsetMethodCount; i++)
        applyOnsetParams(m_onsets[i], cfg);

    if (m_pitch)
    {
        aubio_pitch_set_silence(m_pitch, cfg.pitchSilenceDb);
        aubio_pitch_set_tolerance(m_pitch, cfg.pitchTolerance);
    }

    if (m_notes)
    {
        aubio_notes_set_silence(m_notes, smpl_t(cfg.noteSilenceDb));
        aubio_notes_set_minioi_ms(m_notes, smpl_t(cfg.noteMinIntervalMs));
        aubio_notes_set_release_drop(m_notes, smpl_t(cfg.noteReleaseDropDb));
    }

    if (m_mfcc)
    {
        aubio_mfcc_set_power(m_mfcc, smpl_t(cfg.mfccPower));
        aubio_mfcc_set_scale(m_mfcc, smpl_t(cfg.mfccScale));
    }

    if (m_tss)
    {
        aubio_tss_set_alpha(m_tss, cfg.tssAlpha);
        aubio_tss_set_beta(m_tss, cfg.tssBeta);
        aubio_tss_set_threshold(m_tss, cfg.tssThreshold);
    }

    if (m_tempo)
    {
        aubio_tempo_set_tatum_signature(m_tempo, cfg.tatumSubdivision);
        aubio_tempo_set_silence(m_tempo, cfg.tempoSilenceDb);
        aubio_tempo_set_threshold(m_tempo, cfg.tempoThreshold);
        aubio_tempo_set_delay_ms(m_tempo, smpl_t(cfg.tempoDelayMs));
    }
}

void AubioProcessor::applyPendingConfig()
{
    QMutexLocker locker(&m_configMutex);
    if (!m_hasPendingConfig)
        return;

    AubioConfig oldCfg = m_config;
    m_config = m_pendingConfig;
    m_hasPendingConfig = false;
    locker.unlock();

    if (!m_initialized)
        return;

    if (needsFullRebuild(oldCfg, m_config))
    {
        // initialize() calls release() and uses m_config for all params.
        const uint32_t sr = m_sampleRate;
        initialize(sr);
    }
    else
    {
        applyParamUpdates(oldCfg, m_config);
    }
}

void AubioProcessor::process(const int16_t *monoSamples, int bufferSize)
{
    applyPendingConfig();

    if (!m_initialized || !monoSamples || bufferSize <= 0)
        return;

    const int hop = int(hopSize());
    const int numHops = bufferSize / hop;

    resetResults();

    for (int h = 0; h < numHops; h++)
    {
        const int16_t *src = monoSamples + h * hop;
        // Pristine PCM: only int16 -> float type conversion. No gain, no DC
        // removal, no clipping. Input gain is an OS/hardware concern.
        for (int i = 0; i < hop; i++)
            m_hopBuffer->data[i] = float(src[i]) / 32768.0f;

        processHop();
        m_totalSamplesProcessed += hop;
    }

    if (m_tempo)
    {
        // Raw aubio output. NO clamping. A zero/negative bpm means aubio hasn't
        // locked yet; consumers must handle that explicitly.
        m_results.bpm = double(aubio_tempo_get_bpm(m_tempo));
        m_results.beatConfidence = double(aubio_tempo_get_confidence(m_tempo));

        // QLC+ DERIVATION (not raw aubio): beatPhase is computed from aubio's
        // last-beat timestamp and period plus our own sample counter.
        if (m_config.beatPhaseSource == AubioConfig::AubioSamples && m_sampleRate > 0)
        {
            const double periodS = double(aubio_tempo_get_period_s(m_tempo));
            if (periodS > 0.0)
            {
                const double lastBeatS = double(aubio_tempo_get_last_s(m_tempo));
                const double currentS = double(m_totalSamplesProcessed) / double(m_sampleRate);
                double phase = std::fmod((currentS - lastBeatS) / periodS, 1.0);
                if (phase < 0.0) phase += 1.0;
                m_results.beatPhase = phase;
            }
        }
        // QlcTimer source is wired by the caller (AudioChannel/snapshot stage).
    }
}

void AubioProcessor::processHop()
{
    // 1. Phase vocoder → spectral frame
    if (!m_pvoc)
        return;
    aubio_pvoc_do(m_pvoc, m_hopBuffer, m_fftGrain);

    // 2. Mel filterbank — last hop wins. Aubio writes one mel vector per hop;
    // we expose it pristine (no averaging, no clamp, no scale).
    if (m_filterbank)
    {
        aubio_filterbank_do(m_filterbank, m_fftGrain, m_melOut);
        for (int i = 0; i < AUBIO_MEL_BANDS; i++)
            m_results.mel[i] = double(m_melOut->data[i]);
    }

    // 3. MFCC — last hop wins (same rationale as mel).
    if (m_mfcc)
    {
        aubio_mfcc_do(m_mfcc, m_fftGrain, m_mfccOut);
        for (int i = 0; i < AUBIO_MFCC_COEFFS; i++)
            m_results.mfcc[i] = double(m_mfccOut->data[i]);
    }

    // 4. Spectral descriptors (last hop wins for instantaneous values)
    if (m_descCentroid)
    {
        aubio_specdesc_do(m_descCentroid, m_fftGrain, m_descOut);
        m_results.centroidHz = aubio_bintofreq(double(m_descOut->data[0]), m_sampleRate, windowSize());
    }
    if (m_descSpread)
    {
        aubio_specdesc_do(m_descSpread, m_fftGrain, m_descOut);
        m_results.spread = double(m_descOut->data[0]);
    }
    if (m_descRolloff)
    {
        aubio_specdesc_do(m_descRolloff, m_fftGrain, m_descOut);
        m_results.rolloffHz = aubio_bintofreq(double(m_descOut->data[0]), m_sampleRate, windowSize());
    }
    if (m_descFlux)
    {
        aubio_specdesc_do(m_descFlux, m_fftGrain, m_descOut);
        m_results.flux = double(m_descOut->data[0]);
    }
    if (m_descHfc)
    {
        aubio_specdesc_do(m_descHfc, m_fftGrain, m_descOut);
        m_results.hfc = double(m_descOut->data[0]);
    }

    // 5. Tempo / beat — OR beat across hops (a beat detected on any hop
    // within this buffer is preserved; tatum uses last-hop semantics).
    if (m_tempo)
    {
        aubio_tempo_do(m_tempo, m_hopBuffer, m_tempoOut);
        if (m_tempoOut->data[0] != 0.0f)
            m_results.beat = true;
        m_results.tatum = aubio_tempo_was_tatum(m_tempo);
    }

    // 6. Pitch — last hop wins. We pass aubio's pitch and confidence through
    // pristinely (no max-confidence selection across hops).
    if (m_pitch)
    {
        aubio_pitch_do(m_pitch, m_hopBuffer, m_pitchOut);
        m_results.pitchHz = double(m_pitchOut->data[0]);
        m_results.pitchConfidence = double(aubio_pitch_get_confidence(m_pitch));
    }

    // 7. Onset detection (9 methods) — OR fires across all hops in this
    // buffer. Diagnostics (descriptor / thresholded descriptor) are last-hop.
    for (int i = 0; i < kOnsetMethodCount; i++)
    {
        if (!m_onsets[i])
            continue;
        aubio_onset_do(m_onsets[i], m_hopBuffer, m_onsetOut);
        const bool fired = (m_onsetOut->data[0] != 0.0f);
        if (fired)
        {
            switch (i) {
            case 0: m_results.onsets.energy = true; break;
            case 1: m_results.onsets.hfc = true; break;
            case 2: m_results.onsets.complex = true; break;
            case 3: m_results.onsets.phase = true; break;
            case 4: m_results.onsets.wphase = true; break;
            case 5: m_results.onsets.specdiff = true; break;
            case 6: m_results.onsets.kl = true; break;
            case 7: m_results.onsets.mkl = true; break;
            case 8: m_results.onsets.specflux = true; break;
            }
        }
        m_results.onsetDescriptors[i] = double(aubio_onset_get_descriptor(m_onsets[i]));
        m_results.onsetThresholdedDescriptors[i] = double(aubio_onset_get_thresholded_descriptor(m_onsets[i]));
    }

    // 8. Notes — OR noteOn/noteOff across hops; last firing hop wins on
    // midi/velocity so callers always get values from the most recent onset.
    if (m_notes)
    {
        aubio_notes_do(m_notes, m_hopBuffer, m_notesOut);
        const bool noteOnHop = (m_notesOut->data[0] != 0.0f);
        if (noteOnHop)
        {
            m_results.noteOn = true;
            m_results.noteMidi = double(m_notesOut->data[0]);
            m_results.noteVelocity = double(m_notesOut->data[1]);
        }
        if (m_notesOut->data[2] != 0.0f)
            m_results.noteOff = true;
    }

    // 9. TSS (transient/steady split) — copy aubio's per-bin cvec norms straight
    // through. Last hop wins. No summing, no ratio. Bin i maps to frequency
    // aubio_bintofreq(i, sampleRate, winSize); consumers do their own derivations.
    if (m_tss)
    {
        aubio_tss_do(m_tss, m_fftGrain, m_transGrain, m_steadGrain);
        const int binCount = std::min<int>(int(m_transGrain->length),
                                           AubioResults::kMaxTssBins);
        m_results.tssBinCount = binCount;
        for (int i = 0; i < binCount; i++)
        {
            m_results.tssTransientNorm[i] = double(m_transGrain->norm[i]);
            m_results.tssSteadyNorm[i]    = double(m_steadGrain->norm[i]);
        }
    }
}

void AubioProcessor::resetResults()
{
    m_results = AubioResults{};
}
