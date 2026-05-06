#include "aubioprocessor.h"

#include <algorithm>
#include <cmath>
#include <cstring>

static const char *kOnsetMethods[9] = {
    "energy", "hfc", "complex", "phase", "wphase",
    "specdiff", "kl", "mkl", "specflux"
};

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

    const QByteArray tempoMethod = m_config.tempoMethod.toUtf8();
    m_tempo = new_aubio_tempo(tempoMethod.constData(), win, hop, sampleRate);
    if (m_tempo)
    {
        aubio_tempo_set_tatum_signature(m_tempo, m_config.tatumSubdivision);
        aubio_tempo_set_silence(m_tempo, m_config.tempoSilenceDb);
        aubio_tempo_set_threshold(m_tempo, m_config.tempoThreshold);
    }

    const QByteArray pitchMethod = m_config.pitchMethod.toUtf8();
    m_pitch = new_aubio_pitch(pitchMethod.constData(), win, hop, sampleRate);
    if (m_pitch)
    {
        aubio_pitch_set_unit(m_pitch, "Hz");
        aubio_pitch_set_silence(m_pitch, m_config.pitchSilenceDb);
        aubio_pitch_set_tolerance(m_pitch, m_config.pitchTolerance);
    }

    m_notes = new_aubio_notes("default", win, hop, sampleRate);

    for (int i = 0; i < kOnsetMethodCount; i++)
    {
        m_onsets[i] = new_aubio_onset(kOnsetMethods[i], win, hop, sampleRate);
        if (m_onsets[i])
        {
            aubio_onset_set_threshold(m_onsets[i], m_config.onsetThreshold);
            aubio_onset_set_minioi_ms(m_onsets[i], m_config.onsetMinIntervalMs);
            aubio_onset_set_silence(m_onsets[i], m_config.onsetSilenceDb);
            if (m_config.onsetDelayMs > 0.0)
                aubio_onset_set_delay_ms(m_onsets[i], m_config.onsetDelayMs);
        }
    }

    m_mfcc = new_aubio_mfcc(win, AUBIO_MEL_BANDS, AUBIO_MFCC_COEFFS, sampleRate);

    m_filterbank = new_aubio_filterbank(AUBIO_MEL_BANDS, win);
    if (m_filterbank)
    {
        // norm must be set BEFORE set_mel_coeffs_slaney (it's read during coeff build).
        aubio_filterbank_set_norm(m_filterbank, smpl_t(m_config.filterbankNorm));
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
    // Algorithm-string changes and filterbank norm changes require destroying
    // and recreating aubio objects. Filterbank norm must be set BEFORE
    // set_mel_coeffs_slaney, so we cannot toggle it at runtime via setters.
    if (o.pitchMethod != n.pitchMethod) return true;
    if (o.tempoMethod != n.tempoMethod) return true;
    if (o.filterbankNorm != n.filterbankNorm) return true;
    return false;
}

void AubioProcessor::applyParamUpdates(const AubioConfig &/*old*/, const AubioConfig &cfg)
{
    if (m_filterbank)
        aubio_filterbank_set_power(m_filterbank, smpl_t(cfg.filterbankPower));

    for (int i = 0; i < kOnsetMethodCount; i++)
    {
        if (m_onsets[i] == nullptr) continue;
        aubio_onset_set_threshold(m_onsets[i], cfg.onsetThreshold);
        aubio_onset_set_minioi_ms(m_onsets[i], cfg.onsetMinIntervalMs);
        aubio_onset_set_silence(m_onsets[i], cfg.onsetSilenceDb);
        if (cfg.onsetDelayMs > 0.0)
            aubio_onset_set_delay_ms(m_onsets[i], cfg.onsetDelayMs);
    }

    if (m_pitch)
    {
        aubio_pitch_set_silence(m_pitch, cfg.pitchSilenceDb);
        aubio_pitch_set_tolerance(m_pitch, cfg.pitchTolerance);
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

    const float gain = float(m_config.inputGainLinear);

    for (int h = 0; h < numHops; h++)
    {
        const int16_t *src = monoSamples + h * hop;
        // PRE-aubio PCM gain: applied to the float-normalized samples before
        // any aubio_*_do() call. Aubio output is pristine downstream.
        for (int i = 0; i < hop; i++)
            m_hopBuffer->data[i] = (float(src[i]) / 32768.0f) * gain;

        processHop();
        m_totalSamplesProcessed += hop;
    }

    if (m_tempo)
    {
        // Raw aubio output. NO clamping. A zero/negative bpm means aubio hasn't
        // locked yet; consumers must handle that explicitly.
        m_results.bpm = double(aubio_tempo_get_bpm(m_tempo));
        m_results.beatConfidence = double(aubio_tempo_get_confidence(m_tempo));

        // beatPhase from sample counter (AubioSamples source).
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

    m_results.onsets.voteCount =
        int(m_results.onsets.energy) + int(m_results.onsets.hfc) +
        int(m_results.onsets.complex) + int(m_results.onsets.phase) +
        int(m_results.onsets.wphase) + int(m_results.onsets.specdiff) +
        int(m_results.onsets.kl) + int(m_results.onsets.mkl) +
        int(m_results.onsets.specflux);
}

void AubioProcessor::processHop()
{
    // 1. Phase vocoder → spectral frame
    aubio_pvoc_do(m_pvoc, m_hopBuffer, m_fftGrain);

    // 2. Mel filterbank — last hop wins. Aubio writes one mel vector per hop;
    // we expose it pristine (no averaging, no clamp, no scale).
    aubio_filterbank_do(m_filterbank, m_fftGrain, m_melOut);
    for (int i = 0; i < AUBIO_MEL_BANDS; i++)
        m_results.mel[i] = double(m_melOut->data[i]);

    // 3. MFCC — last hop wins (same rationale as mel).
    aubio_mfcc_do(m_mfcc, m_fftGrain, m_mfccOut);
    for (int i = 0; i < AUBIO_MFCC_COEFFS; i++)
        m_results.mfcc[i] = double(m_mfccOut->data[i]);

    // 4. Spectral descriptors (last hop wins for instantaneous values)
    aubio_specdesc_do(m_descCentroid, m_fftGrain, m_descOut);
    m_results.centroidHz = double(m_descOut->data[0]) * m_sampleRate / windowSize();
    aubio_specdesc_do(m_descSpread, m_fftGrain, m_descOut);
    m_results.spread = double(m_descOut->data[0]);
    aubio_specdesc_do(m_descRolloff, m_fftGrain, m_descOut);
    m_results.rolloffHz = double(m_descOut->data[0]) * m_sampleRate / windowSize();
    aubio_specdesc_do(m_descFlux, m_fftGrain, m_descOut);
    m_results.flux = double(m_descOut->data[0]);
    aubio_specdesc_do(m_descHfc, m_fftGrain, m_descOut);
    m_results.hfc = double(m_descOut->data[0]);

    // 5. Tempo / beat
    aubio_tempo_do(m_tempo, m_hopBuffer, m_tempoOut);
    if (m_tempoOut->data[0] != 0.0f)
        m_results.beat = true;
    if (aubio_tempo_was_tatum(m_tempo))
        m_results.tatum = true;

    // 6. Pitch — last hop wins. We pass aubio's pitch and confidence through
    // pristinely (no max-confidence selection across hops).
    aubio_pitch_do(m_pitch, m_hopBuffer, m_pitchOut);
    m_results.pitchHz = double(m_pitchOut->data[0]);
    m_results.pitchConfidence = double(aubio_pitch_get_confidence(m_pitch));

    // 7. Onset detection (9 methods, OR across hops)
    for (int i = 0; i < kOnsetMethodCount; i++)
    {
        if (!m_onsets[i])
            continue;
        aubio_onset_do(m_onsets[i], m_hopBuffer, m_onsetOut);
        if (m_onsetOut->data[0] != 0.0f)
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
    }

    // 8. Notes
    aubio_notes_do(m_notes, m_hopBuffer, m_notesOut);
    if (m_notesOut->data[0] != 0.0f)
    {
        m_results.noteOn = true;
        m_results.noteMidi = double(m_notesOut->data[0]);
        m_results.noteVelocity = double(m_notesOut->data[1]);
    }
    if (m_notesOut->data[2] != 0.0f)
        m_results.noteOff = true;

    // 9. RMS / peak — computed by us from the (gain-applied) PCM hop buffer.
    // These are NOT aubio outputs; they are QLC+ PCM measurements. Keep
    // max-over-hops to surface buffer peaks for the noise gate / VU meter.
    double rmsSum = 0.0, peakVal = 0.0;
    for (uint_t i = 0; i < hopSize(); i++)
    {
        double s = double(m_hopBuffer->data[i]);
        rmsSum += s * s;
        peakVal = std::max(peakVal, std::abs(s));
    }
    m_results.rms = std::max(m_results.rms, std::sqrt(rmsSum / hopSize()));
    m_results.peak = std::max(m_results.peak, peakVal);

    // 10. TSS (transient/steady split) — last hop wins. We sum aubio's
    // transient/steady cvec norms into per-hop totals; transientRatio is the
    // last-hop ratio. No max/avg across hops.
    aubio_tss_do(m_tss, m_fftGrain, m_transGrain, m_steadGrain);

    double tE = 0.0, sE = 0.0;
    for (uint_t i = 0; i < m_fftGrain->length; i++)
    {
        tE += double(m_transGrain->norm[i]);
        sE += double(m_steadGrain->norm[i]);
    }
    const double total = tE + sE + 1e-10;
    m_results.transientEnergy = tE / total;
    m_results.steadyEnergy = sE / total;
    m_results.transientRatio = tE / total;
}

void AubioProcessor::resetResults()
{
    m_results = AubioResults{};
}
