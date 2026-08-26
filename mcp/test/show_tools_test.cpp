/*
  Q Light Controller Plus - Unit test
  show_tools_test.cpp

  Copyright (C) Massimo Callegari

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#include <QtTest>
#include <QBuffer>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <nlohmann/json.hpp>

#include "show_tools_test.h"
#include "tool_registry.h"
#include "doc.h"
#include "scene.h"
#include "show.h"
#include "showfunction.h"
#include "track.h"
#include <QSet>

#include <fastmcpp/tools/manager.hpp>

using Json = nlohmann::json;

namespace {

Json parsed(const Json &value)
{
    if (value.is_string())
        return Json::parse(value.get<std::string>());
    return value;
}

Json invoke(Doc *doc, const char *tool, const Json &args)
{
    fastmcpp::tools::ToolManager tm;
    registerShowTools(tm, doc);
    return parsed(tm.invoke(tool, args));
}

Scene *makeScene(Doc *doc, const QString &name, uint fadeIn = 0, uint hold = 2000)
{
    Scene *scene = new Scene(doc);
    scene->setName(name);
    scene->setFadeInSpeed(fadeIn);
    scene->setDuration(hold);
    doc->addFunction(scene);
    return scene;
}

Show *makeShow(Doc *doc, const QString &name)
{
    Json result = invoke(doc, "create_shows", Json{{"items", Json::array({
        {{"name", name.toStdString()}, {"tracks", Json::array({{{"name", "Track 1"}}})}}
    })}});
    return qobject_cast<Show*>(doc->function((quint32)result[0].value("id", -1)));
}

}

void McpShowTools_Test::init()
{
    m_doc = new Doc(this);
}

void McpShowTools_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
}

// ─── create_shows ──────────────────────────────────────────────────────────

void McpShowTools_Test::createShows_createsShowWithTracks()
{
    Json result = invoke(m_doc, "create_shows", Json{{"items", Json::array({
        {{"name", "Summer Set"}, {"path", "Shows/2026"},
         {"tracks", Json::array({{{"name", "Wash"}}, {{"name", "Beams"}}, {{"name", "Haze"}}})}}
    })}});

    QVERIFY2(!result[0].contains("error"), result[0].dump().c_str());
    QCOMPARE(result[0].value("status", std::string()), std::string("created"));
    QCOMPARE(result[0]["tracks"].size(), (size_t)3);

    Show *show = qobject_cast<Show*>(m_doc->function((quint32)result[0].value("id", -1)));
    QVERIFY(show != NULL);
    QCOMPARE(show->getTracksCount(), 3);
    // Function::setPath prefixes the type folder.
    QCOMPARE(show->path(), QString("Show/Shows/2026"));
}

void McpShowTools_Test::createShows_upsertsByNameKeepingId()
{
    Json first = invoke(m_doc, "create_shows", Json{{"items", Json::array({
        {{"name", "Set"}, {"tracks", Json::array({{{"name", "Wash"}}})}}
    })}});
    const int id = first[0].value("id", -1);

    Json second = invoke(m_doc, "create_shows", Json{{"items", Json::array({
        {{"name", "Set"}, {"tracks", Json::array({{{"name", "Beams"}}})}}
    })}});

    QCOMPARE(second[0].value("status", std::string()), std::string("updated"));
    QCOMPARE(second[0].value("id", -1), id);

    // One show, and the second call added a track rather than replacing the show.
    int shows = 0;
    for (Function *fn : m_doc->functions())
        if (fn && fn->type() == Function::ShowType) shows++;
    QCOMPARE(shows, 1);
    QCOMPARE(qobject_cast<Show*>(m_doc->function((quint32)id))->getTracksCount(), 2);
}

void McpShowTools_Test::createShows_existingTracksNotDuplicated()
{
    invoke(m_doc, "create_shows", Json{{"items", Json::array({
        {{"name", "Set"}, {"tracks", Json::array({{{"name", "Wash"}}})}}
    })}});

    Json again = invoke(m_doc, "create_shows", Json{{"items", Json::array({
        {{"name", "Set"}, {"tracks", Json::array({{{"name", "Wash"}, {"mute", true}}})}}
    })}});

    QCOMPARE(again[0]["tracks"][0].value("status", std::string()), std::string("updated"));

    Show *show = qobject_cast<Show*>(m_doc->function((quint32)again[0].value("id", -1)));
    QCOMPARE(show->getTracksCount(), 1);
    QCOMPARE(show->tracks().first()->isMute(), true);
}

void McpShowTools_Test::createShows_beatDivision_setsBpm()
{
    Json result = invoke(m_doc, "create_shows", Json{{"items", Json::array({
        {{"name", "Club"}, {"tempoType", "4/4"}, {"bpm", 128}}
    })}});

    QVERIFY2(!result[0].contains("error"), result[0].dump().c_str());

    Show *show = qobject_cast<Show*>(m_doc->function((quint32)result[0].value("id", -1)));
    QCOMPARE(show->timeDivisionBPM(), 128);
    QVERIFY(!Show::isTimeBasedDivision(show->timeDivisionType()));
}

// ─── add_show_items ────────────────────────────────────────────────────────

void McpShowTools_Test::addShowItems_placesOnTimeline()
{
    Show *show = makeShow(m_doc, "Set");
    Scene *scene = makeScene(m_doc, "Wash");

    Json result = invoke(m_doc, "add_show_items", Json{
        {"showID", (int)show->id()}, {"trackName", "Track 1"},
        {"items", Json::array({
            {{"functionID", (int)scene->id()}, {"startTime", 5000}, {"duration", 3000}}
        })}
    });

    QCOMPARE(result[0].value("status", std::string()), std::string("added"));
    QCOMPARE(result[0].value("startTime", -1), 5000);
    QCOMPARE(result[0].value("duration", -1), 3000);

    Track *track = show->tracks().first();
    QCOMPARE(track->showFunctions().count(), 1);
    QCOMPARE((int)track->showFunctions().first()->startTime(), 5000);
}

void McpShowTools_Test::addShowItems_overlap_data()
{
    QTest::addColumn<int>("start");
    QTest::addColumn<int>("duration");
    QTest::addColumn<bool>("accepted");

    // An existing item occupies [1000, 3000).
    QTest::newRow("before, abutting")   << 0    << 1000 << true;
    QTest::newRow("after, abutting")    << 3000 << 1000 << true;
    QTest::newRow("overlaps head")      << 500  << 1000 << false;
    QTest::newRow("overlaps tail")      << 2500 << 1000 << false;
    QTest::newRow("contained")          << 1500 << 500  << false;
    QTest::newRow("contains existing")  << 500  << 3000 << false;
    QTest::newRow("exact same span")    << 1000 << 2000 << false;
    QTest::newRow("well clear")         << 9000 << 1000 << true;
}

void McpShowTools_Test::addShowItems_overlap()
{
    QFETCH(int, start);
    QFETCH(int, duration);
    QFETCH(bool, accepted);

    Show *show = makeShow(m_doc, "Set");
    Scene *first = makeScene(m_doc, "First");
    Scene *second = makeScene(m_doc, "Second");

    invoke(m_doc, "add_show_items", Json{
        {"showID", (int)show->id()}, {"trackName", "Track 1"},
        {"items", Json::array({
            {{"functionID", (int)first->id()}, {"startTime", 1000}, {"duration", 2000}}
        })}
    });

    Json result = invoke(m_doc, "add_show_items", Json{
        {"showID", (int)show->id()}, {"trackName", "Track 1"},
        {"items", Json::array({
            {{"functionID", (int)second->id()}, {"startTime", start}, {"duration", duration}}
        })}
    });

    const int expected = accepted ? 2 : 1;
    QVERIFY2(result[0].contains("error") != accepted, result[0].dump().c_str());
    QCOMPARE(show->tracks().first()->showFunctions().count(), expected);
}

void McpShowTools_Test::addShowItems_defaultDurationFromFunction()
{
    Show *show = makeShow(m_doc, "Set");
    Scene *scene = makeScene(m_doc, "Wash", 500, 2500);

    Json result = invoke(m_doc, "add_show_items", Json{
        {"showID", (int)show->id()}, {"trackName", "Track 1"},
        {"items", Json::array({{{"functionID", (int)scene->id()}, {"startTime", 0}}})}
    });

    // No duration given: the item must take the function's own length, not a
    // hard-coded default.
    QCOMPARE(result[0].value("duration", -1), (int)scene->totalDuration());
    QVERIFY(result[0].value("duration", 0) > 0);
}

void McpShowTools_Test::addShowItems_missingTrack_isCreated()
{
    Show *show = makeShow(m_doc, "Set");
    Scene *scene = makeScene(m_doc, "Wash");

    invoke(m_doc, "add_show_items", Json{
        {"showName", "Set"}, {"trackName", "Brand New"},
        {"items", Json::array({{{"functionName", "Wash"}, {"startTime", 0}}})}
    });

    QCOMPARE(show->getTracksCount(), 2);
    Track *created = show->tracks().last();
    QCOMPARE(created->name(), QString("Brand New"));
    QCOMPARE(created->showFunctions().count(), 1);
    QCOMPARE(created->showFunctions().first()->functionID(), scene->id());
}

void McpShowTools_Test::addShowItems_showOnItsOwnTimeline_rejected()
{
    Show *show = makeShow(m_doc, "Set");

    Json result = invoke(m_doc, "add_show_items", Json{
        {"showID", (int)show->id()}, {"trackName", "Track 1"},
        {"items", Json::array({{{"functionID", (int)show->id()}, {"startTime", 0}}})}
    });

    QVERIFY2(result[0].contains("error"), result[0].dump().c_str());
    QCOMPARE(show->tracks().first()->showFunctions().count(), 0);
}

void McpShowTools_Test::addShowItems_unknownFunction_error()
{
    Show *show = makeShow(m_doc, "Set");

    Json result = invoke(m_doc, "add_show_items", Json{
        {"showID", (int)show->id()}, {"trackName", "Track 1"},
        {"items", Json::array({{{"functionID", 4242}, {"startTime", 0}}})}
    });

    QVERIFY2(result[0].contains("error"), result[0].dump().c_str());
}

// ─── query / delete ────────────────────────────────────────────────────────

void McpShowTools_Test::queryShows_reportsTracksAndItems()
{
    Show *show = makeShow(m_doc, "Set");
    Scene *a = makeScene(m_doc, "A");
    Scene *b = makeScene(m_doc, "B");

    invoke(m_doc, "add_show_items", Json{
        {"showID", (int)show->id()}, {"trackName", "Track 1"},
        {"items", Json::array({
            {{"functionID", (int)a->id()}, {"startTime", 0}, {"duration", 1000}},
            {{"functionID", (int)b->id()}, {"startTime", 4000}, {"duration", 1000}}
        })}
    });
    invoke(m_doc, "add_show_items", Json{
        {"showID", (int)show->id()}, {"trackName", "Track 2"},
        {"items", Json::array({{{"functionID", (int)a->id()}, {"startTime", 2000}, {"duration", 500}}})}
    });

    Json read = invoke(m_doc, "query_shows", Json::object());

    QCOMPARE(read.size(), (size_t)1);
    QCOMPARE(read[0]["tracks"].size(), (size_t)2);
    // Track item counts differ, so this proves per-track reporting.
    QCOMPARE(read[0]["tracks"][0]["items"].size(), (size_t)2);
    QCOMPARE(read[0]["tracks"][1]["items"].size(), (size_t)1);
    QCOMPARE(read[0]["tracks"][0]["items"][1].value("startTime", -1), 4000);
    QCOMPARE(read[0]["tracks"][0]["items"][1].value("functionName", std::string()), std::string("B"));
}

void McpShowTools_Test::showItemIds_areDistinctAcrossTracks()
{
    Show *show = makeShow(m_doc, "Set");
    Scene *a = makeScene(m_doc, "A");
    Scene *b = makeScene(m_doc, "B");

    Json first = invoke(m_doc, "add_show_items", Json{
        {"showID", (int)show->id()}, {"trackName", "Track 1"},
        {"items", Json::array({
            {{"functionID", (int)a->id()}, {"startTime", 0}, {"duration", 1000}},
            {{"functionID", (int)b->id()}, {"startTime", 2000}, {"duration", 1000}}
        })}
    });
    Json second = invoke(m_doc, "add_show_items", Json{
        {"showID", (int)show->id()}, {"trackName", "Track 2"},
        {"items", Json::array({{{"functionID", (int)a->id()}, {"startTime", 0}, {"duration", 1000}}})}
    });

    // Track::createShowFunction allocates ids from its parent Show. An
    // unparented track hands every item id 0, which makes delete_show_items
    // delete whichever item it finds first.
    QSet<int> ids;
    ids << first[0].value("id", -1) << first[1].value("id", -1) << second[0].value("id", -1);
    QCOMPARE(ids.size(), 3);
}

void McpShowTools_Test::deleteShowItems_deletesTheNamedItemOnly()
{
    Show *show = makeShow(m_doc, "Set");
    Scene *a = makeScene(m_doc, "A");
    Scene *b = makeScene(m_doc, "B");

    invoke(m_doc, "add_show_items", Json{
        {"showID", (int)show->id()}, {"trackName", "Track 1"},
        {"items", Json::array({{{"functionID", (int)a->id()}, {"startTime", 0}, {"duration", 1000}}})}
    });
    Json target = invoke(m_doc, "add_show_items", Json{
        {"showID", (int)show->id()}, {"trackName", "Track 2"},
        {"items", Json::array({{{"functionID", (int)b->id()}, {"startTime", 0}, {"duration", 1000}}})}
    });

    invoke(m_doc, "delete_show_items", Json{
        {"showID", (int)show->id()}, {"itemIDs", Json::array({target[0].value("id", -1)})}
    });

    // Track 1 must be untouched; only the item on Track 2 goes.
    QCOMPARE(show->tracks().at(0)->showFunctions().count(), 1);
    QCOMPARE(show->tracks().at(1)->showFunctions().count(), 0);
}

void McpShowTools_Test::createShows_updatePath_marksModified()
{
    invoke(m_doc, "create_shows", Json{{"items", Json::array({{{"name", "Set"}}})}});
    m_doc->resetModified();

    invoke(m_doc, "create_shows", Json{{"items", Json::array({
        {{"name", "Set"}, {"path", "Shows/2026"}, {"tracks", Json::array({{{"name", "New"}}})}}
    })}});

    // Show::setPath and addTrack notify nothing Doc listens to, so without an
    // explicit setModified these edits would be discarded without warning.
    QCOMPARE(m_doc->isModified(), true);
}

void McpShowTools_Test::createShows_invalidField_leavesNothingBehind()
{
    Json result = invoke(m_doc, "create_shows", Json{{"items", Json::array({
        {{"name", "Good"}},
        {{"name", "Bad"}, {"tracks", "not an array"}}
    })}});

    // A malformed item must be a per-item error, not a thrown batch abort, and
    // must not leave a partly-built show in the project.
    QCOMPARE(result.size(), (size_t)2);
    QVERIFY2(!result[0].contains("error"), result[0].dump().c_str());
    QVERIFY2(result[1].contains("error"), result[1].dump().c_str());

    int shows = 0;
    for (Function *fn : m_doc->functions())
        if (fn && fn->type() == Function::ShowType) shows++;
    QCOMPARE(shows, 1);
}

void McpShowTools_Test::createShows_missingName_isPerItemError()
{
    Json result = invoke(m_doc, "create_shows", Json{{"items", Json::array({
        {{"name", "Good"}}, {{"path", "orphan"}}
    })}});

    QCOMPARE(result.size(), (size_t)2);
    QVERIFY2(!result[0].contains("error"), result[0].dump().c_str());
    QVERIFY2(result[1].contains("error"), result[1].dump().c_str());
}

void McpShowTools_Test::queryShows_tempoTypeFeedsBackIntoCreate()
{
    invoke(m_doc, "create_shows", Json{{"items", Json::array({
        {{"name", "Club"}, {"tempoType", "3/4"}, {"bpm", 128}}
    })}});

    Json read = invoke(m_doc, "query_shows", Json::object());
    const std::string tempo = read[0].value("tempoType", std::string());
    QCOMPARE(tempo, std::string("3/4"));

    // Read-modify-write has to be possible: what query reports must be a value
    // create_shows accepts.
    Json again = invoke(m_doc, "create_shows", Json{{"items", Json::array({
        {{"name", "Club"}, {"tempoType", tempo}}
    })}});
    QVERIFY2(!again[0].contains("error"), again[0].dump().c_str());
}

void McpShowTools_Test::addShowItems_ambiguousFunctionName_rejected()
{
    Show *show = makeShow(m_doc, "Set");
    makeScene(m_doc, "Blue");
    makeScene(m_doc, "Blue");

    Json result = invoke(m_doc, "add_show_items", Json{
        {"showID", (int)show->id()}, {"trackName", "Track 1"},
        {"items", Json::array({{{"functionName", "Blue"}, {"startTime", 0}}})}
    });

    // Function names are not unique; binding to whichever came first would be
    // silently wrong.
    QVERIFY2(result[0].contains("error"), result[0].dump().c_str());
    QCOMPARE(show->tracks().first()->showFunctions().count(), 0);
}

void McpShowTools_Test::deleteShowItems_removesItemKeepsFunction()
{
    Show *show = makeShow(m_doc, "Set");
    Scene *scene = makeScene(m_doc, "Wash");

    Json added = invoke(m_doc, "add_show_items", Json{
        {"showID", (int)show->id()}, {"trackName", "Track 1"},
        {"items", Json::array({{{"functionID", (int)scene->id()}, {"startTime", 0}, {"duration", 1000}}})}
    });
    const int itemID = added[0].value("id", -1);

    Json result = invoke(m_doc, "delete_show_items", Json{
        {"showID", (int)show->id()}, {"itemIDs", Json::array({itemID})}
    });

    QCOMPARE(result[0].value("status", std::string()), std::string("deleted"));
    QCOMPARE(show->tracks().first()->showFunctions().count(), 0);
    // The Scene itself must survive — only the timeline placement was removed.
    QVERIFY(m_doc->function(scene->id()) != NULL);
}

void McpShowTools_Test::deleteShowItems_removesWholeTrack()
{
    Show *show = makeShow(m_doc, "Set");
    Scene *scene = makeScene(m_doc, "Wash");

    invoke(m_doc, "add_show_items", Json{
        {"showID", (int)show->id()}, {"trackName", "Doomed"},
        {"items", Json::array({{{"functionID", (int)scene->id()}, {"startTime", 0}, {"duration", 1000}}})}
    });
    QCOMPARE(show->getTracksCount(), 2);

    Json result = invoke(m_doc, "delete_show_items", Json{
        {"showID", (int)show->id()}, {"trackNames", Json::array({"Doomed"})}
    });

    QCOMPARE(result[0].value("status", std::string()), std::string("deleted"));
    QCOMPARE(show->getTracksCount(), 1);
    QVERIFY(m_doc->function(scene->id()) != NULL);
}

// ─── persistence ───────────────────────────────────────────────────────────

void McpShowTools_Test::show_survivesXmlRoundTrip()
{
    Show *show = makeShow(m_doc, "Set");
    Scene *scene = makeScene(m_doc, "Wash");

    invoke(m_doc, "add_show_items", Json{
        {"showID", (int)show->id()}, {"trackName", "Track 1"},
        {"items", Json::array({{{"functionID", (int)scene->id()},
                                {"startTime", 7250}, {"duration", 1750}}})}
    });

    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);
    QXmlStreamWriter writer(&buffer);
    writer.writeStartDocument();
    QVERIFY(m_doc->saveXML(&writer));
    writer.writeEndDocument();
    buffer.close();

    Doc reloaded(this);
    QXmlStreamReader reader(data);
    reader.readNextStartElement();
    QVERIFY(reloaded.loadXML(reader));

    Show *loaded = NULL;
    for (Function *fn : reloaded.functions())
        if (fn && fn->type() == Function::ShowType) loaded = qobject_cast<Show*>(fn);
    QVERIFY(loaded != NULL);
    QCOMPARE(loaded->getTracksCount(), 1);

    QList<ShowFunction*> items = loaded->tracks().first()->showFunctions();
    QCOMPARE(items.count(), 1);
    // Timings must survive to the millisecond.
    QCOMPARE((int)items.first()->startTime(), 7250);
    QCOMPARE((int)items.first()->duration(), 1750);
}

QTEST_MAIN(McpShowTools_Test)
