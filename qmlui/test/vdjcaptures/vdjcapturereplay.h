/*
  Q Light Controller Plus - Unit test
  vdjcapturereplay.h

  Licensed under the Apache License, Version 2.0
*/

#ifndef VDJCAPTUREREPLAY_H
#define VDJCAPTUREREPLAY_H

#include "djfsm.h"
#include "vdjtelemetryclient.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

/**
 * Replays a VirtualDJ telemetry capture (qmlui/test/vdjcaptures, one .jsonl per capture) into
 * a VdjTelemetryClient without a socket. A capture is one JSON record per
 * line: {"t":ms,"ev":"rx","data":"<raw chunk>"} as the socket delivered it,
 * a {"ev":"meta","state":[[trigger,value],...]} record holding every value VDJ
 * had sent before the slice in its order (fed first, as a snapshot), and
 * marks. Captures were
 * taken on VDJ 8.5.9307 and anonymized: paths are /music/track-N.mp3, titles
 * "Title N", artists "Artist N"; timings and structure are the originals.
 */
namespace VdjCapture
{

inline QString path(const char *name)
{
    return QStringLiteral(VDJ_CAPTURE_DIR "/%1.jsonl").arg(QLatin1String(name));
}

/** Feeds the snapshot and every rx chunk; the number of chunks fed, -1 when the file is missing */
inline int replay(VdjTelemetryClient &client, const char *name)
{
    QFile file(path(name));
    if (!file.open(QIODevice::ReadOnly))
        return -1;
    int chunks = 0;
    while (!file.atEnd())
    {
        const QJsonObject record = QJsonDocument::fromJson(file.readLine()).object();
        const QString ev = record.value(QLatin1String("ev")).toString();
        if (ev == QLatin1String("meta") && record.contains(QLatin1String("state")))
        {
            QByteArray snapshot;
            for (const QJsonValue &pair : record.value(QLatin1String("state")).toArray())
            {
                QJsonObject object{ { QStringLiteral("evt"), QStringLiteral("subscribed") },
                                    { QStringLiteral("trigger"), pair.toArray().at(0) },
                                    { QStringLiteral("value"), pair.toArray().at(1) } };
                snapshot += QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
            }
            client.feed(snapshot);
        }
        else if (ev == QLatin1String("rx"))
        {
            client.feed(record.value(QLatin1String("data")).toString().toUtf8());
            chunks++;
        }
    }
    return chunks;
}

/** The client's triggers reach a DjFsm as VdjBridge routes them */
inline void wire(VdjTelemetryClient &client, DjFsm &fsm)
{
    QObject::connect(&client, &VdjTelemetryClient::deckTriggerReceived, &fsm,
                     [&fsm](int deckIndex, const QString &trigger, const QVariant &value) {
                         fsm.onDeckTrigger(deckIndex + 1, trigger, value);
                     });
    QObject::connect(&client, &VdjTelemetryClient::globalTriggerReceived, &fsm,
                     [&fsm](const QString &trigger, const QVariant &value) {
                         if (trigger == QLatin1String("get_decks"))
                             fsm.setDeckCount(value.toInt());
                     });
}

/** The client's triggers reach a VdjBridge through its (private) plugin slots */
inline void wire(VdjTelemetryClient &client, QObject *bridge)
{
    QObject::connect(&client, SIGNAL(deckTriggerReceived(int,QString,QVariant)),
                     bridge, SLOT(onDeckTrigger(int,QString,QVariant)));
    QObject::connect(&client, SIGNAL(globalTriggerReceived(QString,QVariant)),
                     bridge, SLOT(onGlobalTrigger(QString,QVariant)));
}

} // namespace VdjCapture

#endif // VDJCAPTUREREPLAY_H
