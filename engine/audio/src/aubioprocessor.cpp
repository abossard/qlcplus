#include "aubioprocessor.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <QStringList>
#include <QDebug>

// Onset method names: single source of truth lives in aubioprocessor.h
// (AubioProcessor::kOnsetMethodNames). Local alias keeps call sites tidy.
static constexpr const char *const *kOnsetMethods = AubioProcessor::kOnsetMethodNames;

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

// matt_mel warp (LedFx default, melbank.py:264-287):
//   matt = 3700 * log_12(1 + f/230)
// Compared to standard mel (2595 * log10(1 + f/700)) the 230 Hz pivot and
// base-12 log give ~3x more resolution under 500 Hz while still reaching
// 15 kHz — better low-frequency resolution for the 3-bank split.
inline double hzToMattMel(double hz)
{
    return 3700.0 * std::log(1.0 + hz / 230.0) / std::log(12.0);
}
inline double mattMelToHz(double mat)
{
    return 230.0 * (std::pow(12.0, mat / 3700.0) - 1.0);
}

// Build aubio matt_mel triangle bands on `fb`. Generates `nBands + 2`
// boundary frequencies (lower edge, N centres, upper edge — same convention
// LedFx uses on melbank.py:272-286), evenly spaced in matt_mel space and
// converted back to Hz. aubio_filterbank_set_triangle_bands() copies its
// fvec input internally, so we free `freqs` immediately after the call.
void setMattMelBands(aubio_filterbank_t *fb,
                     double fmin, double fmax,
                     int nBands, uint32_t sampleRate)
{
    if (!fb || nBands <= 0)
        return;
    fvec_t *freqs = new_fvec(uint_t(nBands + 2));
    if (!freqs)
        return;
    const double mattMin = hzToMattMel(fmin);
    const double mattMax = hzToMattMel(fmax);
    for (int i = 0; i < nBands + 2; ++i)
    {
        const double t   = double(i) / double(nBands + 1);
        const double mat = mattMin + (mattMax - mattMin) * t;
        freqs->data[i] = smpl_t(mattMelToHz(mat));
    }
    aubio_filterbank_set_triangle_bands(fb, freqs, smpl_t(sampleRate));
    del_fvec(freqs);
}

// Apply per-method onset overrides on top of aubio's defaults. Sentinel
// values (set in audiochannelconfig.h) leave aubio's per-method tuning
// untouched. Real values are forwarded directly to aubio_onset_set_*().
void applyOnsetOverride(aubio_onset_t *onset, const OnsetMethodOverride &ov)
{
    if (!onset)
        return;
    if (ov.threshold >= 0.0)
        aubio_onset_set_threshold(onset, smpl_t(ov.threshold));
    if (ov.silenceDb > -900.0)
        aubio_onset_set_silence(onset, smpl_t(ov.silenceDb));
    if (ov.minioiMs >= 0.0)
        aubio_onset_set_minioi_ms(onset, smpl_t(ov.minioiMs));
    if (ov.delayMs > -9000.0)
        aubio_onset_set_delay_ms(onset, smpl_t(ov.delayMs));
    if (ov.compression >= 0.0)
        aubio_onset_set_compression(onset, smpl_t(ov.compression));
    if (ov.awhitening >= 0)
        aubio_onset_set_awhitening(onset, ov.awhitening ? 1 : 0);
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

    const uint_t win = windowSize();
    const uint_t hop = hopSize();

    m_pvoc = new_aubio_pvoc(win, hop);
    if (m_pvoc)
        aubio_pvoc_set_window(m_pvoc, sanitizedWindowType(m_config.windowType).constData());

    m_preEmphasis = new_aubio_filter(3);
    if (m_preEmphasis)
        aubio_filter_set_biquad(m_preEmphasis,
                                0.8268, -1.6536, 0.8268,
                                -1.6536, 0.6536);

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
        // Pitch unit forwarded directly to aubio. Output in m_results.pitchHz
        // is whatever unit aubio returns for the configured unit string —
        // consumers (QML) inspect AubioConfig::pitchUnit for formatting.
        const QByteArray unit = m_config.pitchUnit.toUtf8();
        aubio_pitch_set_unit(m_pitch, unit.isEmpty() ? "Hz" : unit.constData());
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
        // new_aubio_onset() invokes aubio_onset_set_default_parameters()
        // internally for the chosen method. We then layer per-method
        // overrides (threshold/silence/minioi/delay/compression/awhitening)
        // ONLY where the user has set a real value — sentinel values keep
        // aubio's per-method tuning intact.
        m_onsets[i] = new_aubio_onset(kOnsetMethods[i], win, hop, sampleRate);
        applyOnsetOverride(m_onsets[i], m_config.onsetOverrides[i]);
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

    // Multi-resolution mel banks. Three nested matt_mel banks share m_fftGrain
    // (read-only consumers — see aubio_filterbank_do). Each bank is built with
    // norm BEFORE set_triangle_bands (norm is read during coeff build), then
    // power applied last. Power=1.0 here per RD review: LedFx-style power
    // scaling is done in MelPostProcessor on the QLC+ side, not inside aubio.
    // Bands beyond the per-bank `bands` count remain zero in m_results. The
    // 3 banks are always built — there is no enable toggle.
    {
        auto buildBank = [&](aubio_filterbank_t **fb, fvec_t **out,
                             const MelBankConfig::Bank &bank)
        {
            const int n = std::clamp(bank.bands, 1, MelBankConfig::kMaxBandsPerBank);
            *fb = new_aubio_filterbank(uint_t(n), win);
            if (*fb)
            {
                aubio_filterbank_set_norm(*fb, smpl_t(m_config.filterbankNorm));
                setMattMelBands(*fb, bank.minHz, bank.maxHz, n, sampleRate);
                aubio_filterbank_set_power(*fb, smpl_t(m_config.filterbankPower));
            }
            *out = new_fvec(uint_t(n));
        };
        buildBank(&m_filterbankLow,  &m_melLowOut,  m_config.melBanks.low);
        buildBank(&m_filterbankMid,  &m_melMidOut,  m_config.melBanks.mid);
        buildBank(&m_filterbankHigh, &m_melHighOut, m_config.melBanks.high);
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

    // Beat phase state — reset only here (and in release()), per RD review.
    m_beatPeriodS = 0.0;
    m_lastBeatTimeS = 0.0;
    m_processedSamples = 0;
    m_hopsSinceBeat = 0;
    m_barBeatCount = -1;
    m_beatsPerBar = std::clamp(m_config.beatsPerBar, 1, 8);

    // Required-allocation gate. Optional algorithms (tempo/pitch/notes/onsets/
    // descriptors/mel banks) keep their existing per-call nil guards, but
    // these ones are dereferenced unconditionally in process()/processHop().
    const bool ok =
        m_pvoc && m_hopBuffer && m_fftGrain &&
        m_filterbank && m_melOut &&
        m_mfcc && m_mfccOut &&
        m_descOut && m_tss && m_transGrain && m_steadGrain &&
        m_onsetOut && m_tempoOut && m_pitchOut && m_notesOut;
    if (!ok)
    {
        qCritical() << "AubioProcessor::initialize: allocation failed; "
                       "audio analysis disabled";
        release();
        m_initialized = false;
        return;
    }

    m_initialized = true;
}

void AubioProcessor::release()
{
    if (m_pvoc) { del_aubio_pvoc(m_pvoc); m_pvoc = nullptr; }
    if (m_preEmphasis) { del_aubio_filter(m_preEmphasis); m_preEmphasis = nullptr; }
    if (m_tempo) { del_aubio_tempo(m_tempo); m_tempo = nullptr; }
    if (m_pitch) { del_aubio_pitch(m_pitch); m_pitch = nullptr; }
    if (m_notes) { del_aubio_notes(m_notes); m_notes = nullptr; }
    if (m_mfcc) { del_aubio_mfcc(m_mfcc); m_mfcc = nullptr; }
    if (m_filterbank) { del_aubio_filterbank(m_filterbank); m_filterbank = nullptr; }
    if (m_filterbankLow)  { del_aubio_filterbank(m_filterbankLow);  m_filterbankLow  = nullptr; }
    if (m_filterbankMid)  { del_aubio_filterbank(m_filterbankMid);  m_filterbankMid  = nullptr; }
    if (m_filterbankHigh) { del_aubio_filterbank(m_filterbankHigh); m_filterbankHigh = nullptr; }
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
    if (m_melLowOut)  { del_fvec(m_melLowOut);  m_melLowOut  = nullptr; }
    if (m_melMidOut)  { del_fvec(m_melMidOut);  m_melMidOut  = nullptr; }
    if (m_melHighOut) { del_fvec(m_melHighOut); m_melHighOut = nullptr; }
    if (m_mfccOut) { del_fvec(m_mfccOut); m_mfccOut = nullptr; }
    if (m_descOut) { del_fvec(m_descOut); m_descOut = nullptr; }
    if (m_transGrain) { del_cvec(m_transGrain); m_transGrain = nullptr; }
    if (m_steadGrain) { del_cvec(m_steadGrain); m_steadGrain = nullptr; }

    m_beatPeriodS = 0.0;
    m_lastBeatTimeS = 0.0;
    m_processedSamples = 0;
    m_hopsSinceBeat = 0;
    m_barBeatCount = -1;
    m_beatsPerBar = 4;

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
    // MelBankConfig is baked into the 3 matt_mel filterbanks at init time —
    // any change to enabled / minHz / maxHz / bands per bank requires a full
    // rebuild. Filterbank power is handled in applyParamUpdates().
    if (o.melBanks != n.melBanks) return true;
    return false;
}

void AubioProcessor::applyParamUpdates(const AubioConfig &oldCfg, const AubioConfig &cfg)
{
    if (m_filterbank)
        aubio_filterbank_set_power(m_filterbank, smpl_t(cfg.filterbankPower));
    // Mel banks share the same filterbankPower (kept at 1.0 by default — see
    // RD note on power scaling living in MelPostProcessor, not aubio).
    if (m_filterbankLow)
        aubio_filterbank_set_power(m_filterbankLow,  smpl_t(cfg.filterbankPower));
    if (m_filterbankMid)
        aubio_filterbank_set_power(m_filterbankMid,  smpl_t(cfg.filterbankPower));
    if (m_filterbankHigh)
        aubio_filterbank_set_power(m_filterbankHigh, smpl_t(cfg.filterbankPower));

    // Targeted onset enable/disable: only create/destroy the detectors whose
    // enabled flag actually changed. Avoids tearing down the rest of the
    // pipeline when toggling a single method. Per-method overrides are
    // re-applied here whenever the override values changed (or the detector
    // was just (re)created), so users can tune live without a full rebuild.
    for (int i = 0; i < kOnsetMethodCount; i++)
    {
        const bool wasEnabled = oldCfg.onsetMethodEnabled[i];
        const bool nowEnabled = cfg.onsetMethodEnabled[i];
        if (wasEnabled != nowEnabled)
        {
            if (m_onsets[i])
            {
                del_aubio_onset(m_onsets[i]);
                m_onsets[i] = nullptr;
            }
            if (nowEnabled)
            {
                m_onsets[i] = new_aubio_onset(kOnsetMethods[i],
                                              windowSize(), hopSize(), m_sampleRate);
                applyOnsetOverride(m_onsets[i], cfg.onsetOverrides[i]);
            }
        }
        else if (nowEnabled && m_onsets[i])
        {
            const OnsetMethodOverride &oldOv = oldCfg.onsetOverrides[i];
            const OnsetMethodOverride &newOv = cfg.onsetOverrides[i];
            const bool changed =
                oldOv.threshold != newOv.threshold ||
                oldOv.silenceDb != newOv.silenceDb ||
                oldOv.minioiMs != newOv.minioiMs ||
                oldOv.delayMs != newOv.delayMs ||
                oldOv.compression != newOv.compression ||
                oldOv.awhitening != newOv.awhitening;
            if (changed)
            {
                // A reset-to-default (sentinel) cannot be expressed as a
                // single setter call — recreate the detector so aubio's
                // per-method defaults are re-applied, then layer overrides.
                del_aubio_onset(m_onsets[i]);
                m_onsets[i] = new_aubio_onset(kOnsetMethods[i],
                                              windowSize(), hopSize(), m_sampleRate);
                applyOnsetOverride(m_onsets[i], newOv);
            }
        }
    }

    if (m_pitch)
    {
        const QByteArray unit = cfg.pitchUnit.toUtf8();
        aubio_pitch_set_unit(m_pitch, unit.isEmpty() ? "Hz" : unit.constData());
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

    if (m_preEmphasis && oldCfg.preEmphasisEnabled != cfg.preEmphasisEnabled)
        aubio_filter_do_reset(m_preEmphasis);

    m_beatsPerBar = std::clamp(cfg.beatsPerBar, 1, 8);
    if (m_barBeatCount >= m_beatsPerBar)
        m_barBeatCount = -1;
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

    const bool rebuild = needsFullRebuild(oldCfg, m_config);
    if (rebuild)
    {
        // initialize() calls release() and uses m_config for all params.
        const uint32_t sr = m_sampleRate;
        initialize(sr);
    }
    else
    {
        applyParamUpdates(oldCfg, m_config);
    }

#ifdef AUDIO_DEBUG
    qDebug().nospace() << "[AubioProcessor] config applied: pitch=" << m_config.pitchMethod
        << " mel_power=" << m_config.filterbankPower
        << " rebuild=" << (rebuild ? "true" : "false");
#endif
}

void AubioProcessor::process(const int16_t *monoSamples, int bufferSize)
{
    applyPendingConfig();

    if (!m_initialized || !monoSamples || bufferSize <= 0)
        return;

    const int hop = int(hopSize());
    if (bufferSize < hop)
        return;

    resetResults();

    // One hop in, one process call out — exactly as aubio's own examples do
    // (examples/utils.c -> process_func per aubio_source_do). No batching,
    // no aggregation: aubio's algorithms are stateful and expect to be fed
    // hop-by-hop.
    // PCM input: int16 -> float type conversion only, then optional LedFx-style
    // pre-emphasis below. No gain, no DC removal, no clipping.
    for (int i = 0; i < hop; i++)
        m_hopBuffer->data[i] = float(monoSamples[i]) / 32768.0f;

    if (m_config.preEmphasisEnabled && m_preEmphasis)
        aubio_filter_do(m_preEmphasis, m_hopBuffer);

    processHop();

    if (m_tempo)
    {
        // Raw aubio output. NO clamping. A zero/negative bpm means aubio hasn't
        // locked yet; consumers must handle that explicitly.
        m_results.bpm = double(aubio_tempo_get_bpm(m_tempo));
        m_results.beatConfidence = double(aubio_tempo_get_confidence(m_tempo));
    }

#ifdef AUDIO_DEBUG
    static int _aubioDbg = 0;
    if (++_aubioDbg >= 25)
    {
        _aubioDbg = 0;

        // 1. Mel range
        double minMel = m_results.mel[0], maxMel = m_results.mel[0], sumMel = 0;
        for (int i = 0; i < AUBIO_MEL_BANDS; ++i) {
            minMel = std::min(minMel, m_results.mel[i]);
            maxMel = std::max(maxMel, m_results.mel[i]);
            sumMel += m_results.mel[i];
        }
        qDebug().nospace() << "[aubio:mel] min=" << minMel << " max=" << maxMel
            << " avg=" << (sumMel / AUBIO_MEL_BANDS);

        // 2. Onsets
        qDebug().nospace() << "[aubio:onset] "
            << (m_results.onsets.energy ? "E " : "-- ")
            << (m_results.onsets.hfc ? "H " : "-- ")
            << (m_results.onsets.complex ? "C " : "-- ")
            << (m_results.onsets.phase ? "P " : "-- ")
            << (m_results.onsets.wphase ? "WP " : "-- ")
            << (m_results.onsets.specdiff ? "SD " : "-- ")
            << (m_results.onsets.kl ? "KL " : "-- ")
            << (m_results.onsets.mkl ? "MKL " : "-- ")
            << (m_results.onsets.specflux ? "SF " : "-- ")
            << "| desc: " << m_results.onsetDescriptors[0] << " " << m_results.onsetDescriptors[1];

        // 3. Tempo
        qDebug().nospace() << "[aubio:tempo] bpm=" << m_results.bpm
            << " conf=" << m_results.beatConfidence
            << " beat=" << m_results.beat << " phase=" << m_results.beatPhase;

        // 4. Pitch
        qDebug().nospace() << "[aubio:pitch] hz=" << m_results.pitchHz
            << " conf=" << m_results.pitchConfidence;

        // 5. MFCC
        qDebug().nospace() << "[aubio:mfcc] c0=" << m_results.mfcc[0]
            << " c1=" << m_results.mfcc[1] << " c2=" << m_results.mfcc[2]
            << " c3=" << m_results.mfcc[3] << " c4=" << m_results.mfcc[4];

        // 6. TSS
        double transMax = 0, steadMax = 0;
        for (int i = 0; i < m_results.tssBinCount; ++i) {
            transMax = std::max(transMax, m_results.tssTransientNorm[i]);
            steadMax = std::max(steadMax, m_results.tssSteadyNorm[i]);
        }
        qDebug().nospace() << "[aubio:tss] transMax=" << transMax
            << " steadMax=" << steadMax << " bins=" << m_results.tssBinCount;
    }
#endif
}

void AubioProcessor::processHop()
{
    // 1. Phase vocoder → spectral frame
    if (!m_pvoc)
        return;
    aubio_pvoc_do(m_pvoc, m_hopBuffer, m_fftGrain);

    // 2. Mel filterbank
    if (m_filterbank)
    {
        aubio_filterbank_do(m_filterbank, m_fftGrain, m_melOut);
        for (int i = 0; i < AUBIO_MEL_BANDS; i++)
            m_results.mel[i] = double(m_melOut->data[i]);
    }

    // 2b. Multi-resolution mel banks. Three independent filterbanks consume
    // m_fftGrain read-only — no extra FFT, no cvec copy. Per-bank output is
    // written into AubioResults::mel{Low,Mid,High}; *Count records the valid
    // prefix length so consumers don't read stale tail values when the band
    // count is reconfigured to a smaller number.
    auto runBank = [&](aubio_filterbank_t *fb, fvec_t *out,
                       double *dst, int &count)
    {
        if (!fb || !out)
        {
            count = 0;
            return;
        }
        aubio_filterbank_do(fb, m_fftGrain, out);
        const int n = std::min<int>(int(out->length), AUBIO_MELBANK_MAX);
        for (int i = 0; i < n; ++i)
            dst[i] = double(out->data[i]);
        count = n;
    };
    runBank(m_filterbankLow,  m_melLowOut,
            m_results.melLow,  m_results.melLowCount);
    runBank(m_filterbankMid,  m_melMidOut,
            m_results.melMid,  m_results.melMidCount);
    runBank(m_filterbankHigh, m_melHighOut,
            m_results.melHigh, m_results.melHighCount);

    // TEMP forensic debug: dump raw mel sums/maxes every ~200 frames (~4s @ 50Hz).
    {
        static int dbgCounter = 0;
        if (++dbgCounter % 200 == 0)
        {
            double sumLow=0, sumMid=0, sumHigh=0;
            double maxLow=0, maxMid=0, maxHigh=0;
            for (int i = 0; i < m_results.melLowCount;  i++) {
                sumLow  += m_results.melLow[i];
                if (m_results.melLow[i]  > maxLow)  maxLow  = m_results.melLow[i];
            }
            for (int i = 0; i < m_results.melMidCount;  i++) {
                sumMid  += m_results.melMid[i];
                if (m_results.melMid[i]  > maxMid)  maxMid  = m_results.melMid[i];
            }
            for (int i = 0; i < m_results.melHighCount; i++) {
                sumHigh += m_results.melHigh[i];
                if (m_results.melHigh[i] > maxHigh) maxHigh = m_results.melHigh[i];
            }
            qDebug() << "[FORENSIC] RAW MEL sums:" << sumLow << sumMid << sumHigh
                     << "maxes:" << maxLow << maxMid << maxHigh
                     << "counts:" << m_results.melLowCount
                                  << m_results.melMidCount
                                  << m_results.melHighCount;
        }
    }

    // 3. MFCC
    if (m_mfcc)
    {
        aubio_mfcc_do(m_mfcc, m_fftGrain, m_mfccOut);
        for (int i = 0; i < AUBIO_MFCC_COEFFS; i++)
            m_results.mfcc[i] = double(m_mfccOut->data[i]);
    }

    // 4. Spectral descriptors
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

    // 5. Tempo / beat
    if (m_tempo)
    {
        aubio_tempo_do(m_tempo, m_hopBuffer, m_tempoOut);
        m_results.beat = (m_tempoOut->data[0] != 0.0f);
        m_results.tatum = aubio_tempo_was_tatum(m_tempo);

        // --- Beat phase (LedFx-style 0→1 ramp synced to BPM) ---
        // Use the processed-sample counter as our stream clock; aubio reports
        // beat timestamps in the same stream-time domain via get_last_s().
        const double currentTimeS =
            double(m_processedSamples) / double(m_sampleRate);

        if (m_results.beat)
        {
            // Snapshot aubio's authoritative period and last-beat timestamp
            // (RD: prefer aubio_tempo_get_last_s() over a local hop clock —
            // accounts for aubio's internal peak-pick delay/onset latency).
            m_beatPeriodS  = double(aubio_tempo_get_period_s(m_tempo));
            m_lastBeatTimeS = double(aubio_tempo_get_last_s(m_tempo));
            m_hopsSinceBeat = 0;
            m_barBeatCount = (m_barBeatCount + 1) % std::max(1, m_beatsPerBar);
        }
        else
        {
            ++m_hopsSinceBeat;
        }

        // Gate phase to 0 when no beats have fired for several beat periods —
        // prevents a runaway "ghost" ramp during silence or before tempo lock.
        // 4 beat periods of silence (≈345 hops at 120 BPM/44.1k/hop=512) is a
        // generous bound that still keeps phase live across short audio drops.
        const uint32_t silenceHopLimit =
            (m_beatPeriodS > 0.0)
                ? uint32_t(4.0 * m_beatPeriodS * double(m_sampleRate) / double(hopSize()))
                : 0u;

        if (m_beatPeriodS > 0.0 && m_hopsSinceBeat <= silenceHopLimit)
        {
            const double elapsed = currentTimeS - m_lastBeatTimeS;
            const double phase = elapsed / m_beatPeriodS;
            // Wrap into [0, 1) — handles dropouts and minor period drift.
            m_results.beatPhase = phase - std::floor(phase);
        }
        else
        {
            m_results.beatPhase = 0.0;
        }
        m_results.barPhase = double(m_barBeatCount) + m_results.beatPhase;
    }

    // 6. Pitch
    if (m_pitch)
    {
        aubio_pitch_do(m_pitch, m_hopBuffer, m_pitchOut);
        m_results.pitchHz = double(m_pitchOut->data[0]);
        m_results.pitchConfidence = double(aubio_pitch_get_confidence(m_pitch));
    }

    // 7. Onset detection (9 methods). aubio's internal minioi_ms prevents
    // rapid re-fire across hops, so consumers don't need extra debouncing.
    for (int i = 0; i < kOnsetMethodCount; i++)
    {
        if (!m_onsets[i])
            continue;
        aubio_onset_do(m_onsets[i], m_hopBuffer, m_onsetOut);
        const bool fired = (m_onsetOut->data[0] != 0.0f);
        switch (i) {
        case 0: m_results.onsets.energy = fired; break;
        case 1: m_results.onsets.hfc = fired; break;
        case 2: m_results.onsets.complex = fired; break;
        case 3: m_results.onsets.phase = fired; break;
        case 4: m_results.onsets.wphase = fired; break;
        case 5: m_results.onsets.specdiff = fired; break;
        case 6: m_results.onsets.kl = fired; break;
        case 7: m_results.onsets.mkl = fired; break;
        case 8: m_results.onsets.specflux = fired; break;
        default: break;
        }
        m_results.onsetDescriptors[i] = double(aubio_onset_get_descriptor(m_onsets[i]));
        m_results.onsetThresholdedDescriptors[i] = double(aubio_onset_get_thresholded_descriptor(m_onsets[i]));
    }

    // 8. Notes
    if (m_notes)
    {
        aubio_notes_do(m_notes, m_hopBuffer, m_notesOut);
        const bool noteOnHop = (m_notesOut->data[0] != 0.0f);
        m_results.noteOn = noteOnHop;
        if (noteOnHop)
        {
            m_results.noteMidi = double(m_notesOut->data[0]);
            m_results.noteVelocity = double(m_notesOut->data[1]);
        }
        m_results.noteOff = (m_notesOut->data[2] != 0.0f);
    }

    // 9. TSS (transient/steady split). aubio's per-bin cvec norms passed
    // straight through. Bin i maps to aubio_bintofreq(i, sampleRate, winSize);
    // consumers do their own derivations.
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

    // Stream-time bookkeeping for beat phase (one hop = hopSize() samples).
    m_processedSamples += hopSize();
}

void AubioProcessor::resetResults()
{
    m_results = AubioResults{};
}

OnsetMethodOverride readAubioOnsetDefaults(int methodIndex, uint32_t sampleRate)
{
    OnsetMethodOverride def;
    if (methodIndex < 0 || methodIndex >= AubioProcessor::kOnsetMethodCount)
        return def;
    // Spin up a one-shot aubio_onset_t purely to read the per-method tuned
    // defaults. aubio applies those when new_aubio_onset() is constructed
    // (see set_default_parameters() inside libaubio), so the getters here
    // return aubio's authoritative values.
    aubio_onset_t *o = new_aubio_onset(kOnsetMethods[methodIndex],
                                       AubioProcessor::windowSize(),
                                       AubioProcessor::hopSize(),
                                       sampleRate);
    if (!o)
        return def;
    def.threshold = double(aubio_onset_get_threshold(o));
    def.silenceDb = double(aubio_onset_get_silence(o));
    def.minioiMs = double(aubio_onset_get_minioi_ms(o));
    def.delayMs = double(aubio_onset_get_delay_ms(o));
    def.compression = double(aubio_onset_get_compression(o));
    def.awhitening = aubio_onset_get_awhitening(o) != 0 ? 1 : 0;
    del_aubio_onset(o);
    return def;
}
