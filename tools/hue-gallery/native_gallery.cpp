#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSemaphore>
#include "doc.h"
#include "huematrix.h"
#include "huescript.h"
#include "huescriptscache.h"
#include "audiosnapshot.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>

static void saveJson(const QString &path, const QJsonDocument &data)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data.toJson()) < 0)
        qFatal("Cannot write JSON");
}

static void drainJS()
{
    QSemaphore ready;
    if (!HUEScript::scheduleOnJSThread([&] { ready.release(); }) ||
        !ready.tryAcquire(1, 10000))
        qFatal("Cannot drain JS thread");
}

static AudioSnapshot snapshotAt(int frame)
{
    const double t = frame / 50.0;
    const double cycle = std::fmod(t, 4.0);
    const double phase = std::fmod(t * 2, 1.0);
    const bool quiet = cycle < 0.5;
    const bool sustain = cycle >= 0.5 && cycle < 2.0;
    const double kick = quiet ? 0 : std::exp(-phase * 13);
    const double envelope = quiet ? 0.035 : sustain ? 0.56 : 0.82;
    AudioSnapshot s;
    s.sourceId = "hue-gallery-synthetic-120bpm";
    s.profileId = 7;
    s.sourceEpoch = 1;
    s.configRevision = 1;
    s.frameSequence = frame + 1;
    s.publishTimeNs = 1000000000LL + int64_t(frame) * 20000000LL;
    s.sampleTime = uint64_t(frame) * 960;
    s.available = true;
    s.status = "available";
    s.noiseGateClosed = quiet;
    s.audioDtMs = 20;
    s.volume.raw = envelope * 0.22;
    s.volume.volumeNorm = envelope;
    s.features.rmsDb = 20 * std::log10(std::max(0.000001, s.volume.raw));
    s.features.peakDb = s.features.rmsDb + 7;
    s.beatPower = std::min(1.0, envelope * 0.18 + kick * 0.9);
    s.bassPower = envelope * (0.52 + 0.28 * std::sin(t * 2.7));
    s.lows = (s.beatPower + s.bassPower) * 0.5;
    s.mids = envelope * (0.52 + 0.24 * std::sin(t * 3.1 + 0.8));
    s.highs = std::min(0.98, envelope * (0.43 + 0.32 * std::sin(t * 6.3 + 1.3))
        + (!quiet && !sustain ? kick * 0.72 : 0));
    s.powersRaw[0] = std::min(1.0, 0.08 * envelope + kick * 0.92);
    s.powersRaw[1] = envelope * (0.44 + 0.33 * std::sin(t * 2.7 + 0.4));
    s.powersRaw[2] = envelope * (0.60 + 0.28 * std::sin(t * 3.1 + 1.0));
    s.powersRaw[3] = envelope * (0.35 + 0.28 * std::sin(t * 6.3 + 0.2));
    s.music.valid = true;
    s.music.bpm = 120;
    s.music.beatConfidence = 0.96;
    s.music.beatPhase = phase;
    s.music.barPhase = std::fmod(t / 2.0, 1.0);
    s.music.beatInBar = int(t * 2) % 4;
    s.music.beatsPerBar = 4;
    s.events.beat = frame / 25;
    s.events.bar = frame / 100;
    for (int f = 25; f <= frame; f += 25)
        if ((f % 200) >= 25)
            ++s.events.kick;
    for (int f = 10; f <= frame; f += 10)
        if ((f % 200) >= 25)
            ++s.events.onset;
    s.onsets.thresholdedDescriptors[
        std::clamp(s.config.aubio.onsetMethodIndex, 0, AUBIO_ONSET_METHODS - 1)] =
        quiet ? 0 : kick * 0.95;
    const int semitones[] = {0, 3, 7, 10, 12, 7, 5, 3};
    s.pitch.hz = quiet ? 0 : 130.8128 * std::pow(2.0, semitones[(frame / 25) % 8] / 12.0);
    s.pitch.confidence = quiet ? 0 : 0.94;
    AudioSnapshot::MelBankSnapshot *banks[] = {&s.melLow, &s.melMid, &s.melHigh};
    const double ranges[3][2] = {{30, 250}, {250, 3000}, {30, 10000}};
    for (int b = 0; b < 3; ++b)
    {
        auto &bank = *banks[b];
        bank.count = 24;
        bank.minHz = ranges[b][0];
        bank.maxHz = ranges[b][1];
        bank.gain = 1;
        for (int i = 0; i < bank.count; ++i)
        {
            const double x = double(i) / (bank.count - 1);
            const double wave = 0.5 + 0.5 * std::sin(x * 13 + t * (1.2 + b * 0.4));
            const double peak = std::exp(-std::pow((x - (0.35 + 0.22 * std::sin(t * 1.7))) * 9, 2));
            bank.centersHz[i] = bank.minHz * std::pow(bank.maxHz / bank.minHz, x);
            bank.raw[i] = envelope * (0.06 + 0.35 * wave + 0.28 * peak);
            bank.processed[i] = std::clamp(envelope * (0.12 + 0.65 * wave + 0.3 * peak), 0.0, 1.0);
            bank.novelty[i] = std::clamp(envelope * (0.05 + 0.52 * wave + 0.3 * kick * peak), 0.0, 1.0);
        }
    }
    return s;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (argc != 5)
        qFatal("Usage: hue_gallery_native inventory|capture scripts plan-or-output capture-root");
    const QString operation = QString::fromLocal8Bit(argv[1]);
    const QDir scriptsDir(QString::fromLocal8Bit(argv[2]));
    Doc doc(nullptr);
    if (!doc.hueScriptsCache()->load(scriptsDir, true))
        qFatal("Cannot load native scripts cache");
    if (operation == "inventory")
    {
        QJsonArray effects;
        for (const auto &filename : scriptsDir.entryList({"*.js"}, QDir::Files, QDir::Name))
        {
            if (filename == "hsvutil.js")
                continue;
            HUEScript script(&doc);
            if (!script.load(scriptsDir.filePath(filename)))
                qFatal("Cannot evaluate script %s", qPrintable(filename));
            QJsonArray properties;
            for (const auto &property : script.properties())
                properties.append(QJsonObject{
                    {"name", property.m_name}, {"label", property.m_displayName},
                    {"type", int(property.m_type)}, {"choices", QJsonArray::fromStringList(property.m_listValues)},
                    {"default", script.property(property.m_name)}});
            effects.append(QJsonObject{{"file", filename}, {"name", script.name()},
                {"audio", script.usesAudio()}, {"acceptColors", script.acceptColors()},
                {"properties", properties}});
        }
        saveJson(QString::fromLocal8Bit(argv[3]), QJsonDocument(effects));
        return 0;
    }
    if (operation != "capture")
        qFatal("Unknown operation");
    QFile plan(QString::fromLocal8Bit(argv[3]));
    if (!plan.open(QIODevice::ReadOnly))
        qFatal("Cannot open capture plan");
    const QJsonArray entries = QJsonDocument::fromJson(plan.readAll()).array();
    const QDir root(QString::fromLocal8Bit(argv[4]));
    QJsonArray reports;
    for (const auto &value : entries)
    {
        const auto entry = value.toObject();
        const QString id = entry["id"].toString();
        const QDir destination(root.filePath(id));
        if (!QDir().mkpath(destination.path()))
            qFatal("Cannot create case output");
        auto *holder = new HUEMatrix(&doc);
        if (!doc.addFunction(holder))
            qFatal("Cannot register isolated matrix owner");
        auto *script = new HUEScript(&doc);
        if (!script->load(scriptsDir.filePath(QFileInfo(entry["sourcePath"].toString()).fileName())))
            qFatal("Cannot load capture script");
        holder->setAlgorithm(script);
        const auto palette = entry["palette"].toArray();
        for (int c = 0; c < palette.size(); ++c)
            holder->setColor(c, QColor(palette[c].toString()));
        const auto settings = entry["settings"].toObject();
        for (auto it = settings.begin(); it != settings.end(); ++it)
            if (!script->setProperty(it.key(), it.value().toVariant().toString()))
                qFatal("Cannot set capture property %s", qPrintable(it.key()));
        const auto layout = entry["layout"].toObject();
        const QSize size(layout["columns"].toInt(), layout["rows"].toInt());
        script->setDisplaySize(size);
        drainJS();
        const int warmup = 200;
        const int total = entry["durationMs"].toInt() / 20;
        const int stride = 50 / entry["fps"].toInt();
        const int steps = std::max(1, script->rgbMapStepCount(size));
        const uint primary = QColor(palette[0].toString()).rgb();
        QFile raw(destination.filePath("frames.rgb"));
        if (!raw.open(QIODevice::WriteOnly))
            qFatal("Cannot write RGB stream");
        QJsonArray pixelProof;
        QJsonArray inputProof;
        QJsonArray frameWallTimes;
        AudioEventCursor proofCursor;
        const auto start = std::chrono::steady_clock::now();
        int captured = 0;
        for (int frame = 0; frame < warmup + total; ++frame)
        {
            RGBMap map;
            const int step = (frame / 25) % steps;
            if (script->usesAudio())
            {
                const auto snapshot = snapshotAt(frame);
                auto audio = AudioRenderView::fromSnapshot(snapshot, snapshot.publishTimeNs);
                auto proofAudio = audio;
                proofCursor.advance(proofAudio);
                script->rgbMapWithAudio(size, primary, step, map, audio, size);
                if (frame % 25 == 0)
                    inputProof.append(QJsonObject::fromVariantMap(audioViewToVariant(proofAudio)));
            }
            else
            {
                std::this_thread::sleep_until(start + std::chrono::milliseconds(frame * 20));
                script->rgbMap(size, primary, step, map);
            }
            if (map.size() != size.height())
                qFatal("Invalid native map height in %s", qPrintable(id));
            for (const auto &row : map)
                if (row.size() != size.width())
                    qFatal("Invalid native map width in %s", qPrintable(id));
            if (frame < warmup || (frame - warmup) % stride != 0)
                continue;
            QByteArray bytes;
            QJsonArray picks;
            for (int y = 0; y < size.height(); ++y)
                for (int x = 0; x < size.width(); ++x)
                {
                    const uint p = map[y][x];
                    bytes.append(char((p >> 16) & 255));
                    bytes.append(char((p >> 8) & 255));
                    bytes.append(char(p & 255));
                    if ((x == 0 && y == 0) || (x == size.width() / 2 && y == size.height() / 2) ||
                        (x == size.width() - 1 && y == size.height() - 1))
                        picks.append(QJsonObject{{"x", x}, {"y", y}, {"rgb", double(p & 0xffffff)}});
                }
            if (raw.write(bytes) != bytes.size())
                qFatal("Cannot write native frame bytes");
            frameWallTimes.append(std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - start).count());
            pixelProof.append(QJsonObject{{"frame", captured}, {"pixels", picks}});
            ++captured;
        }
        raw.close();
        QJsonArray actualColors;
        for (const auto &color : holder->getColors())
            actualColors.append(color.name());
        const auto report = QJsonObject{{"id", id}, {"frames", captured},
            {"width", size.width()}, {"height", size.height()}, {"fps", entry["fps"]},
            {"warmupMs", warmup * 20}, {"audio", script->usesAudio()},
            {"renderIntervalMs", 20}, {"frameWallTimesMs", frameWallTimes},
            {"acceptedColorCount", std::max(0, script->acceptColors())},
            {"paletteOnRegisteredOwner", actualColors}, {"nativeMapPixelProof", pixelProof},
            {"wallMs", double(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count())}};
        saveJson(destination.filePath("native-report.json"), QJsonDocument(report));
        saveJson(destination.filePath("audio-input.json"), QJsonDocument(inputProof));
        reports.append(report);
        script->postRun();
        if (!doc.deleteFunction(holder->id()))
            qFatal("Cannot delete isolated owner");
        drainJS();
        if (!doc.functionsByType(Function::HUEMatrixType).isEmpty())
            qFatal("Isolated owner leaked");
    }
    saveJson(root.filePath("native-reports.json"), QJsonDocument(reports));
    return 0;
}
