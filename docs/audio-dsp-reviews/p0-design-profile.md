# P0 Design: AudioProfile document model

## Intent

`AudioProfile` is an engine-layer document object that owns reusable DSP configuration for audio-reactive rendering and triggering. It is independent of Virtual Console widgets. `VCAudioTrigger` widgets edit and monitor profiles, while `RGBMatrix` functions reference profiles by `audioProfileId`.

Resolution chain for consumers:

1. explicit `audioProfileId`
2. default profile
3. first profile in `Doc`
4. anonymous in-memory fallback profile using defaults

## Proposed `engine/src/audioprofile.h`

```cpp
/*
  Q Light Controller Plus
  audioprofile.h
*/

#ifndef AUDIOPROFILE_H
#define AUDIOPROFILE_H

#include <QObject>
#include <QString>
#include <QVector>

class AudioAnalyzer;
class AudioChannelHandle;
class Doc;
class QXmlStreamReader;
class QXmlStreamWriter;

#define KXMLQLCAudioProfile              QStringLiteral("AudioProfile")
#define KXMLQLCAudioProfileID            QStringLiteral("ID")
#define KXMLQLCAudioProfileName          QStringLiteral("Name")
#define KXMLQLCAudioProfileIsDefault     QStringLiteral("IsDefault")
#define KXMLQLCAudioProfileVersion       QStringLiteral("Version")

#define KXMLQLCAudioProfileBands         QStringLiteral("Bands")
#define KXMLQLCAudioProfileBand          QStringLiteral("Band")
#define KXMLQLCAudioProfileEnvelope      QStringLiteral("Envelope")
#define KXMLQLCAudioProfileAgc           QStringLiteral("Agc")
#define KXMLQLCAudioProfileTriggers      QStringLiteral("Triggers")
#define KXMLQLCAudioProfileTrigger       QStringLiteral("Trigger")
#define KXMLQLCAudioProfileVolume        QStringLiteral("Volume")
#define KXMLQLCAudioProfileNoiseGate     QStringLiteral("NoiseGate")

struct EnvelopeConfig
{
    double attackMs = 25.0;
    double releaseMs = 180.0;
};

struct AgcConfig
{
    bool enabled = true;
    double maxGainDb = 18.0;
    double releaseMs = 1500.0;
    double noiseFloorDb = -54.0;
};

struct TriggerConfig
{
    double highThreshold = 0.65;
    double lowThreshold = 0.45;
    double holdMs = 80.0;
    double cooldownMs = 120.0;
};

struct BandLayout
{
    // Upper edge per band. Lower edge is minFrequencyHz.
    double minFrequencyHz = 40.0;
    double subMaxHz = 60.0;
    double bassMaxHz = 250.0;
    double lowMidMaxHz = 500.0;
    double midMaxHz = 2000.0;
    double highMaxHz = 5000.0;
};

struct VolumeConfig
{
    double smoothingMs = 100.0;
};

struct NoiseGateConfig
{
    double thresholdDb = -54.0;
    double holdMs = 120.0;
};

struct AudioChannelConfig
{
    enum Band
    {
        Sub = 0,
        Bass,
        LowMid,
        Mid,
        High,
        BandCount
    };

    BandLayout bands;
    QVector<EnvelopeConfig> envelopes = QVector<EnvelopeConfig>(BandCount);
    AgcConfig agc;
    QVector<TriggerConfig> triggers = QVector<TriggerConfig>(BandCount);
    VolumeConfig volume;
    NoiseGateConfig noiseGate;

    static AudioChannelConfig defaults();
};

class AudioProfile final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(AudioProfile)

    Q_PROPERTY(quint32 id READ id CONSTANT)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(bool isDefault READ isDefault WRITE setDefault NOTIFY defaultChanged)

public:
    explicit AudioProfile(Doc* parent);
    ~AudioProfile() override;

    void copyFrom(const AudioProfile* profile);

    void setId(quint32 id);
    quint32 id() const;
    static quint32 invalidId();

    void setName(const QString& name);
    QString name() const;

    void setDefault(bool isDefault);
    bool isDefault() const;

    void setConfig(const AudioChannelConfig& config);
    const AudioChannelConfig& config() const;
    AudioChannelConfig& mutableConfig();

    AudioChannelHandle* channelHandle() const;
    void bindAnalyzer(AudioAnalyzer* analyzer);
    void releaseAnalyzer();

    static bool loader(QXmlStreamReader& xmlDoc, Doc* doc);
    bool loadXML(QXmlStreamReader& xmlDoc);
    bool saveXML(QXmlStreamWriter* doc) const;

signals:
    void changed(quint32 id);
    void nameChanged();
    void defaultChanged();
    void configChanged();

private:
    Doc* doc() const;
    void emitConfigChanged();

private:
    quint32 m_id = invalidId();
    QString m_name;
    bool m_isDefault = false;
    AudioChannelConfig m_config = AudioChannelConfig::defaults();

    // Move-only analyzer registration. AudioProfile owns this channel.
    AudioChannelHandle* m_channelHandle = nullptr;
};

#endif
```

### Header design notes

- Inherits `QObject`, like `Function` and `FixtureGroup`, so `Doc` can own it and bridge change signals.
- Uses `Q_PROPERTY` only for stable UI-facing fields. DSP config remains a plain value object to keep processing code simple and testable.
- `setId()` is public for the same practical reason as `FixtureGroup::setId()`, but only `Doc` should call it.
- `AudioChannelHandle` is intentionally opaque. Implementation can switch `m_channelHandle` to `std::unique_ptr<AudioChannelHandle>` or a value member once the analyzer API exists.

## Proposed `AudioChannelConfig`

The config is a pure data structure. It should not depend on `Doc`, `RGBMatrix`, or VC widgets.

```cpp
struct AudioChannelConfig
{
    enum Band { Sub = 0, Bass, LowMid, Mid, High, BandCount };

    BandLayout bands;
    QVector<EnvelopeConfig> envelopes; // size == BandCount
    AgcConfig agc;
    QVector<TriggerConfig> triggers;   // size == BandCount
    VolumeConfig volume;
    NoiseGateConfig noiseGate;

    static AudioChannelConfig defaults()
    {
        AudioChannelConfig cfg;
        cfg.envelopes = QVector<EnvelopeConfig>(BandCount);
        cfg.triggers = QVector<TriggerConfig>(BandCount);
        return cfg;
    }
};
```

Processing contract:

- Analyzer emits five normalized band powers in `[0, 1]` using `BandLayout`.
- Envelope smoothing applies per band before trigger evaluation.
- AGC is applied before envelopes and triggers.
- Noise gate mutes band/volume output before AGC when input level is below `thresholdDb` for `holdMs`.
- Volume smoothing applies to full-range level, not to individual bands.

## XML schema

Profiles are direct children of `<Engine>`, alongside fixtures, fixture groups, palettes, and functions.

```xml
<Engine>
  <AudioProfile ID="0" Name="Default" IsDefault="True" Version="1">
    <Bands MinFrequencyHz="40" SubMaxHz="60" BassMaxHz="250" LowMidMaxHz="500" MidMaxHz="2000" HighMaxHz="5000" />

    <Envelope>
      <Band Name="sub" AttackMs="25" ReleaseMs="180" />
      <Band Name="bass" AttackMs="25" ReleaseMs="180" />
      <Band Name="lowMid" AttackMs="25" ReleaseMs="180" />
      <Band Name="mid" AttackMs="25" ReleaseMs="180" />
      <Band Name="high" AttackMs="25" ReleaseMs="180" />
    </Envelope>

    <Agc Enabled="True" MaxGainDb="18" ReleaseMs="1500" NoiseFloorDb="-54" />

    <Triggers>
      <Trigger Band="sub" HighThreshold="0.65" LowThreshold="0.45" HoldMs="80" CooldownMs="120" />
      <Trigger Band="bass" HighThreshold="0.65" LowThreshold="0.45" HoldMs="80" CooldownMs="120" />
      <Trigger Band="lowMid" HighThreshold="0.65" LowThreshold="0.45" HoldMs="80" CooldownMs="120" />
      <Trigger Band="mid" HighThreshold="0.65" LowThreshold="0.45" HoldMs="80" CooldownMs="120" />
      <Trigger Band="high" HighThreshold="0.65" LowThreshold="0.45" HoldMs="80" CooldownMs="120" />
    </Triggers>

    <Volume SmoothingMs="100" />
    <NoiseGate ThresholdDb="-54" HoldMs="120" />
  </AudioProfile>
</Engine>
```

### XML rules

| Field | Rule |
|---|---|
| `AudioProfile/@ID` | Required `quint32`, unique in `Doc::m_audioProfiles`. |
| `AudioProfile/@Name` | Required, non-empty after trim. |
| `AudioProfile/@IsDefault` | Optional boolean, defaults `False`. Save as `True`/`False` to match existing QLC+ style. |
| `AudioProfile/@Version` | Required for new saves. Version `1` for this schema. Missing version loads as `1` during migration. |
| `Bands` | Optional. Missing fields use defaults. Edges must be strictly increasing. |
| `Envelope/Band` | Optional per band. Missing bands use defaults. Unknown band names are skipped with warning. |
| `Agc` | Optional. Missing attributes use defaults. |
| `Triggers/Trigger` | Optional per band. Missing bands use defaults. `LowThreshold <= HighThreshold`. |
| `Volume` | Optional. Missing attributes use defaults. |
| `NoiseGate` | Optional. Missing attributes use defaults. |

## Default values

| Section | Field | Default |
|---|---:|---:|
| Profile | `Name` | `Default` for auto-created profile, otherwise `Audio Profile <id>` |
| Profile | `IsDefault` | `False`, except the first auto-created profile |
| Bands | `MinFrequencyHz` | `40` |
| Bands | `SubMaxHz` | `60` |
| Bands | `BassMaxHz` | `250` |
| Bands | `LowMidMaxHz` | `500` |
| Bands | `MidMaxHz` | `2000` |
| Bands | `HighMaxHz` | `5000` |
| Envelope, all bands | `AttackMs` | `25` |
| Envelope, all bands | `ReleaseMs` | `180` |
| AGC | `Enabled` | `True` |
| AGC | `MaxGainDb` | `18` |
| AGC | `ReleaseMs` | `1500` |
| AGC | `NoiseFloorDb` | `-54` |
| Triggers, all bands | `HighThreshold` | `0.65` |
| Triggers, all bands | `LowThreshold` | `0.45` |
| Triggers, all bands | `HoldMs` | `80` |
| Triggers, all bands | `CooldownMs` | `120` |
| Volume | `SmoothingMs` | `100` |
| Noise gate | `ThresholdDb` | `-54` |
| Noise gate | `HoldMs` | `120` |

## `Doc` registration pattern

Mirror `FixtureGroup` because profiles are simple document objects, and add default-profile resolution.

### `doc.h` additions

```cpp
#include "audioprofile.h"

public:
    bool addAudioProfile(AudioProfile* profile, quint32 id = AudioProfile::invalidId());
    bool deleteAudioProfile(quint32 id);
    AudioProfile* audioProfile(quint32 id) const;
    QList<AudioProfile*> audioProfiles() const;
    AudioProfile* defaultAudioProfile() const;
    AudioProfile* resolveAudioProfile(quint32 id) const;

signals:
    void audioProfileAdded(quint32 id);
    void audioProfileRemoved(quint32 id);
    void audioProfileChanged(quint32 id);
    void defaultAudioProfileChanged(quint32 id);

private slots:
    void slotAudioProfileChanged(quint32 id);

private:
    quint32 createAudioProfileId();
    void normalizeDefaultAudioProfile(quint32 preferredId = AudioProfile::invalidId());

    QMap<quint32, AudioProfile*> m_audioProfiles;
    quint32 m_latestAudioProfileId;
```

### `doc.cpp` behavior

- `clearContents()` deletes profiles after consumers have stopped, or before functions are deleted only if no function destructor resolves profiles. Prefer: functions, fixtures/groups, profiles.
- `addAudioProfile()`:
  - assigns ID via `createAudioProfileId()` when invalid
  - rejects duplicate or invalid IDs
  - sets `profile->setId(id)`
  - connects `profile->changed(quint32)` to `slotAudioProfileChanged(quint32)`
  - inserts into `m_audioProfiles`
  - if the new profile is default, clears `IsDefault` on other profiles
  - if it is the first profile and none is default, marks it default
  - emits `audioProfileAdded(id)` and marks document modified
- `deleteAudioProfile()`:
  - removes and deletes profile
  - emits `audioProfileRemoved(id)`
  - if the deleted profile was default, promotes the first remaining profile
  - does not rewrite `RGBMatrix::audioProfileId`; resolution handles fallback
- `resolveAudioProfile(id)`:
  - returns explicit profile when found
  - otherwise returns `defaultAudioProfile()`
  - otherwise returns first `audioProfiles().first()`
  - otherwise returns `nullptr`; caller uses anonymous fallback defaults
- `loadXML()` recognizes `<AudioProfile>` before `<Function>` so RGBMatrix references can resolve during post-load.
- `saveXML()` writes profiles before functions.

## RGBMatrix reference

Add a stable ID field to `RGBMatrix`:

```cpp
#define KXMLQLCRGBMatrixAudioProfile QStringLiteral("AudioProfile")

Q_PROPERTY(quint32 audioProfileId READ audioProfileId WRITE setAudioProfileId NOTIFY audioProfileIdChanged)

quint32 m_audioProfileId = AudioProfile::invalidId();
```

XML inside RGBMatrix:

```xml
<Function ID="7" Type="RGBMatrix" Name="Audio Tunnel">
  ...
  <AudioProfile>0</AudioProfile>
</Function>
```

Do not duplicate profile config inside the function. The function stores only the reference.

## Migration: old script sliders to `AudioChannelConfig`

Old audio RGB scripts store per-script properties on `RGBMatrix`:

```xml
<Property Name="presetGain" Value="7" />
<Property Name="presetReactivity" Value="7" />
<Property Name="presetFloor" Value="15" />
<Property Name="presetSensitivity" Value="7" />
```

During XML load:

1. Load RGBMatrix properties as today.
2. If script uses audio and has any legacy slider property, create an `AudioProfile` named `<RGBMatrix name> Audio`.
3. Convert sliders into config.
4. Add profile to `Doc` and set `RGBMatrix::audioProfileId`.
5. Keep legacy properties in memory only if needed for old script compatibility; do not save them once `audioProfileId` exists.

| Old slider | Range | Existing meaning | New target field(s) | Conversion |
|---|---:|---|---|---|
| `presetGain` | `1..10` | `0.6 + gain * 0.2` multiplier | `AgcConfig::maxGainDb` | `maxGainDb = 6 + (gain - 1) * 2` → `6..24 dB` |
| `presetReactivity` | `1..10` | faster visual response / filter rise | all `EnvelopeConfig::attackMs`, `releaseMs`; `VolumeConfig::smoothingMs` | `attackMs = 80 - (reactivity - 1) * 7` → `80..17 ms`; `releaseMs = 400 - (reactivity - 1) * 30` → `400..130 ms`; `smoothingMs = 220 - (reactivity - 1) * 18` → `220..58 ms` |
| `presetFloor` | `0..100` | minimum brightness floor | `NoiseGateConfig::thresholdDb`, `AgcConfig::noiseFloorDb` | `thresholdDb = -60 + floor * 0.30` → `-60..-30 dB`; `noiseFloorDb = thresholdDb` |
| `presetSensitivity` | `1..10` | lower trigger threshold is more sensitive | all `TriggerConfig::highThreshold`, `lowThreshold` | `highThreshold = 0.45 - sensitivity * 0.04` → `0.41..0.05`; clamp to `[0.05, 0.95]`; `lowThreshold = max(0.02, highThreshold - 0.15)` |
| missing sliders | n/a | script default | unchanged defaults | Use `AudioChannelConfig::defaults()` first, then apply present values. |

Notes:

- The sensitivity formula mirrors `audio_common.js::triggerThreshold()`.
- The gain conversion intentionally maps old multiplier-style gain onto AGC headroom, not raw sample multiplication.
- Continuous scripts with no `presetSensitivity` keep default trigger config; trigger scripts with no `presetFloor` keep default noise gate.
- Migrations should be deterministic so repeated loads do not create duplicate profiles. Prefer profile lookup by generated name before creating.

## Validation and tests to add with implementation

- Unit-test `AudioChannelConfig::defaults()` with non-trivial assertions for all nested fields.
- Parameterized XML round-trip tests for full profile, sparse profile, invalid band order, and duplicate default profiles.
- Migration tests for representative slider sets, including a continuous script and a trigger script.
- `Doc::resolveAudioProfile()` tests for explicit, default, first-found, and no-profile fallback paths.
