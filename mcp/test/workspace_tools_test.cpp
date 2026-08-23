/*
  Q Light Controller Plus - Unit test
  workspace_tools_test.cpp

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
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <nlohmann/json.hpp>

#include "workspace_tools_test.h"
#include "tool_registry.h"
#include "workspacebridge.h"
#include "doc.h"
#include "fixture.h"
#include "scene.h"
#include "scenevalue.h"
#include "mastertimer.h"
#include "inputoutputmap.h"
#include "functionparent.h"
#include "function.h"
#include <QUrl>

#include <fastmcpp/tools/manager.hpp>

using Json = nlohmann::json;

namespace {

/**
 * Stands in for App, which the tools cannot drive here because it needs a
 * QQuickView. Records the calls the tools make, and when given a Doc also
 * performs the real XML read/write so a save/load round trip can run through
 * the tools themselves rather than around them.
 */
class FakeWorkspaceBridge final : public WorkspaceBridge
{
public:
    QString fileName;
    QStringList saveCalls;
    QStringList loadCalls;
    int newCalls = 0;
    bool succeed = true;
    Doc *doc = nullptr;   // set to make save/load actually touch disk

    QString currentFileName() const override { return fileName; }

    bool newWorkspace() override
    {
        ++newCalls;
        if (!succeed) return false;
        if (doc) doc->clearContents();
        fileName.clear();
        return true;
    }

    bool loadWorkspace(const QString &path) override
    {
        loadCalls.append(path);
        if (!succeed) return false;
        if (doc)
        {
            QFile in(path);
            if (!in.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
            QXmlStreamReader reader(&in);
            reader.readNextStartElement();
            doc->clearContents();
            if (!doc->loadXML(reader)) return false;
            doc->resetModified();
        }
        fileName = path;
        return true;
    }

    bool saveWorkspace(const QString &path) override
    {
        saveCalls.append(path);
        if (!succeed) return false;
        // App::saveWorkspace forces the .qxw suffix; mirror that so the tool's
        // "report what the app ended up with" behaviour is exercised.
        const QString target = path.endsWith(".qxw") ? path : path + ".qxw";
        if (doc)
        {
            QFile out(target);
            if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
            QXmlStreamWriter writer(&out);
            writer.writeStartDocument();
            writer.writeDTD(QStringLiteral("<!DOCTYPE Workspace>"));
            if (!doc->saveXML(&writer)) return false;
            writer.writeEndDocument();
            out.close();
            doc->resetModified();
        }
        fileName = target;
        return true;
    }
};

Json parsed(const Json &value)
{
    if (value.is_string())
        return Json::parse(value.get<std::string>());
    return value;
}

Json invoke(Doc *doc, WorkspaceBridge *bridge, const char *tool, const Json &args)
{
    fastmcpp::tools::ToolManager tm;
    registerWorkspaceTools(tm, doc, bridge);
    return parsed(tm.invoke(tool, args));
}

/** Minimal file that passes the tool's "is this really a workspace" check. */
bool writeWorkspaceFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    file.write("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
               "<!DOCTYPE Workspace>\n"
               "<Workspace/>\n");
    file.close();
    return true;
}

/**
 * Get one function actually running. MasterTimer::startFunction only queues;
 * the function reaches runningFunctions() on the next 25 Hz tick, so the timer
 * has to be running for the live-output guard to see anything.
 */
Function *startSomething(Doc *doc)
{
    Fixture *fxi = new Fixture(doc);
    fxi->setName("Par 1");
    fxi->setUniverse(0);
    fxi->setAddress(0);
    fxi->setChannels(4);
    doc->addFixture(fxi);

    Scene *scene = new Scene(doc);
    scene->setName("Live");
    scene->setValue(SceneValue(fxi->id(), 0, 255));
    doc->addFunction(scene);
    doc->resetModified();

    // The tick claims the universes, so they have to be running or the timer
    // thread blocks on its very first tick and nothing ever starts.
    doc->inputOutputMap()->startUniverses();
    doc->masterTimer()->start();
    // Go through Function::start, not MasterTimer::startFunction: a function
    // with no FunctionParent reports stopped() and the timer drops it again on
    // the same tick.
    scene->start(doc->masterTimer(), FunctionParent::master());
    return scene;
}

/** Any edit marks the Doc dirty, which is what the unsaved guards key on. */
void dirty(Doc *doc)
{
    Scene *scene = new Scene(doc);
    scene->setName("Edit");
    doc->addFunction(scene);
    doc->setModified();
}

}

void McpWorkspaceTools_Test::init()
{
    m_doc = new Doc(this);
    m_doc->resetModified();
}

void McpWorkspaceTools_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
}

// ─── registration ──────────────────────────────────────────────────────────

void McpWorkspaceTools_Test::withoutBridge_toolsNotRegistered()
{
    fastmcpp::tools::ToolManager tm;
    registerWorkspaceTools(tm, m_doc, nullptr);

    // v4 has no workspace bridge; the tools must simply be absent rather than
    // registered and crashing on a null bridge.
    for (const char *name : {"save_workspace", "load_workspace", "new_workspace",
                             "query_workspace_file"})
        QVERIFY2(!tm.has(name), name);
}

// ─── query_workspace_file ──────────────────────────────────────────────────

void McpWorkspaceTools_Test::queryWorkspaceFile_neverSaved_reportsUnsaved()
{
    FakeWorkspaceBridge bridge;

    Json result = invoke(m_doc, &bridge, "query_workspace_file", Json::object());

    QCOMPARE(result.value("saved", true), false);
    QCOMPARE(result.value("path", std::string("x")), std::string());
    QCOMPARE(result.value("modified", true), false);
    QVERIFY(!result.contains("name"));
}

void McpWorkspaceTools_Test::queryWorkspaceFile_savedAndModified_reported()
{
    FakeWorkspaceBridge bridge;
    bridge.fileName = "/tmp/shows/GARAGE.qxw";
    dirty(m_doc);

    Json result = invoke(m_doc, &bridge, "query_workspace_file", Json::object());

    QCOMPARE(result.value("saved", false), true);
    QCOMPARE(result.value("path", std::string()), std::string("/tmp/shows/GARAGE.qxw"));
    QCOMPARE(result.value("name", std::string()), std::string("GARAGE.qxw"));
    QCOMPARE(result.value("modified", false), true);
}

// ─── save_workspace ────────────────────────────────────────────────────────

void McpWorkspaceTools_Test::saveWorkspace_withPath_delegatesPath()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("New Show");

    FakeWorkspaceBridge bridge;

    Json result = invoke(m_doc, &bridge, "save_workspace", Json{{"path", path.toStdString()}});

    QCOMPARE(bridge.saveCalls, QStringList{path});
    QCOMPARE(result.value("status", std::string()), std::string("saved"));
    // Reported path is what the app settled on, suffix included.
    QCOMPARE(result.value("path", std::string()), (path + ".qxw").toStdString());
}

void McpWorkspaceTools_Test::saveWorkspace_noPath_usesCurrentFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("GARAGE.qxw");

    FakeWorkspaceBridge bridge;
    bridge.fileName = path;

    Json result = invoke(m_doc, &bridge, "save_workspace", Json::object());

    QCOMPARE(bridge.saveCalls, QStringList{path});
    QCOMPARE(result.value("status", std::string()), std::string("saved"));
}

void McpWorkspaceTools_Test::saveWorkspace_noPathNeverSaved_returnsError()
{
    FakeWorkspaceBridge bridge;

    Json result = invoke(m_doc, &bridge, "save_workspace", Json::object());

    QVERIFY2(result.contains("error"), result.dump().c_str());
    QVERIFY(bridge.saveCalls.isEmpty());
}

void McpWorkspaceTools_Test::saveWorkspace_bridgeFails_returnsError()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("x.qxw");

    FakeWorkspaceBridge bridge;
    bridge.succeed = false;

    Json result = invoke(m_doc, &bridge, "save_workspace", Json{{"path", path.toStdString()}});

    QVERIFY2(result.contains("error"), result.dump().c_str());
    QCOMPARE(result.value("path", std::string()), path.toStdString());
}

void McpWorkspaceTools_Test::saveWorkspace_emptyPath_rejected()
{
    FakeWorkspaceBridge bridge;
    bridge.fileName = "/tmp/GARAGE.qxw";

    // An empty string must not silently fall back to the current file.
    Json result = invoke(m_doc, &bridge, "save_workspace", Json{{"path", "   "}});

    QVERIFY2(result.contains("error"), result.dump().c_str());
    QVERIFY(bridge.saveCalls.isEmpty());
}

// ─── load_workspace ────────────────────────────────────────────────────────

void McpWorkspaceTools_Test::loadWorkspace_missingFile_returnsErrorWithoutTouchingBridge()
{
    FakeWorkspaceBridge bridge;

    Json result = invoke(m_doc, &bridge, "load_workspace",
                         Json{{"path", "/definitely/not/here.qxw"}});

    QVERIFY2(result.contains("error"), result.dump().c_str());
    QVERIFY2(bridge.loadCalls.isEmpty(), "must not clear the document for a missing file");
}

void McpWorkspaceTools_Test::loadWorkspace_dirtyDocWithoutFlag_refused()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("show.qxw");
    QVERIFY(writeWorkspaceFile(path));

    FakeWorkspaceBridge bridge;
    dirty(m_doc);

    Json result = invoke(m_doc, &bridge, "load_workspace", Json{{"path", path.toStdString()}});

    QVERIFY2(result.contains("error"), result.dump().c_str());
    QCOMPARE(result.value("modified", false), true);
    QVERIFY2(bridge.loadCalls.isEmpty(), "unsaved work must not be discarded silently");
}

void McpWorkspaceTools_Test::loadWorkspace_dirtyDocWithFlag_loads()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("show.qxw");
    QVERIFY(writeWorkspaceFile(path));

    FakeWorkspaceBridge bridge;
    dirty(m_doc);

    Json result = invoke(m_doc, &bridge, "load_workspace",
                         Json{{"path", path.toStdString()}, {"discardUnsaved", true}});

    QCOMPARE(result.value("status", std::string()), std::string("loaded"));
    QCOMPARE(bridge.loadCalls, QStringList{path});
}

void McpWorkspaceTools_Test::loadWorkspace_cleanDoc_loads()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("show.qxw");
    QVERIFY(writeWorkspaceFile(path));

    FakeWorkspaceBridge bridge;

    Json result = invoke(m_doc, &bridge, "load_workspace", Json{{"path", path.toStdString()}});

    QCOMPARE(result.value("status", std::string()), std::string("loaded"));
    QCOMPARE(bridge.loadCalls, QStringList{path});
}

// ─── new_workspace ─────────────────────────────────────────────────────────

void McpWorkspaceTools_Test::newWorkspace_dirtyDocWithoutFlag_refused()
{
    FakeWorkspaceBridge bridge;
    dirty(m_doc);

    Json result = invoke(m_doc, &bridge, "new_workspace", Json::object());

    QVERIFY2(result.contains("error"), result.dump().c_str());
    QCOMPARE(bridge.newCalls, 0);
}

void McpWorkspaceTools_Test::newWorkspace_cleanDoc_resets()
{
    FakeWorkspaceBridge bridge;
    bridge.fileName = "/tmp/shows/GARAGE.qxw";

    Json result = invoke(m_doc, &bridge, "new_workspace", Json::object());

    QCOMPARE(result.value("status", std::string()), std::string("reset"));
    QCOMPARE(bridge.newCalls, 1);
    QVERIFY(bridge.fileName.isEmpty());
}

// ─── real file I/O ─────────────────────────────────────────────────────────

void McpWorkspaceTools_Test::saveThenLoad_roundTripsThroughDisk()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("round-trip.qxw");

    FakeWorkspaceBridge bridge;
    bridge.doc = m_doc;

    Fixture *fxi = new Fixture(m_doc);
    fxi->setName("Par 1");
    fxi->setUniverse(0);
    fxi->setAddress(0);
    fxi->setChannels(6);
    m_doc->addFixture(fxi);

    Scene *scene = new Scene(m_doc);
    scene->setName("Wash");
    scene->setValue(SceneValue(fxi->id(), 0, 255));
    m_doc->addFunction(scene);

    Json saved = invoke(m_doc, &bridge, "save_workspace", Json{{"path", path.toStdString()}});
    QCOMPARE(saved.value("status", std::string()), std::string("saved"));
    QVERIFY(QFile::exists(path));
    QVERIFY(QFileInfo(path).size() > 0);

    // Wipe it, then bring it back through load_workspace.
    Json reset = invoke(m_doc, &bridge, "new_workspace", Json::object());
    QCOMPARE(reset.value("status", std::string()), std::string("reset"));
    QCOMPARE(m_doc->fixtures().count(), 0);
    QCOMPARE(m_doc->functions().count(), 0);

    Json loaded = invoke(m_doc, &bridge, "load_workspace", Json{{"path", path.toStdString()}});
    QCOMPARE(loaded.value("status", std::string()), std::string("loaded"));

    QCOMPARE(m_doc->fixtures().count(), 1);
    QCOMPARE(m_doc->fixtures().first()->name(), QString("Par 1"));
    QCOMPARE(m_doc->functions().count(), 1);
    QCOMPARE(m_doc->functions().first()->name(), QString("Wash"));
}

void McpWorkspaceTools_Test::loadWorkspace_notAWorkspaceFile_refusedBeforeClearing()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("notes.txt");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("just some text, definitely not a project");
    file.close();

    FakeWorkspaceBridge bridge;
    bridge.doc = m_doc;

    Scene *scene = new Scene(m_doc);
    scene->setName("Keep Me");
    m_doc->addFunction(scene);
    m_doc->resetModified();

    Json result = invoke(m_doc, &bridge, "load_workspace", Json{{"path", path.toStdString()}});

    // App::loadWorkspace clears the document before it discovers the file is
    // not a workspace, and cannot roll back — so the tool must refuse first.
    QVERIFY2(result.contains("error"), result.dump().c_str());
    QVERIFY2(bridge.loadCalls.isEmpty(), "must not hand an unloadable file to the app");
    QCOMPARE(m_doc->functions().count(), 1);
}

void McpWorkspaceTools_Test::loadWorkspace_directory_refused()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    FakeWorkspaceBridge bridge;

    Json result = invoke(m_doc, &bridge, "load_workspace", Json{{"path", dir.path().toStdString()}});

    QVERIFY2(result.contains("error"), result.dump().c_str());
    QVERIFY(bridge.loadCalls.isEmpty());
}

void McpWorkspaceTools_Test::loadWorkspace_fileUrl_normalisedForBridge()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("show.qxw");
    QVERIFY(writeWorkspaceFile(path));

    FakeWorkspaceBridge bridge;

    Json result = invoke(m_doc, &bridge, "load_workspace",
                         Json{{"path", QUrl::fromLocalFile(path).toString().toStdString()}});

    // The bridge contract is a plain local path, so the file: form must be
    // stripped here rather than relying on App to strip it again.
    QCOMPARE(result.value("status", std::string()), std::string("loaded"));
    QCOMPARE(bridge.loadCalls, QStringList{path});
}

void McpWorkspaceTools_Test::loadWorkspace_pathNotAString_rejected()
{
    FakeWorkspaceBridge bridge;

    Json result = invoke(m_doc, &bridge, "load_workspace", Json{{"path", 42}});

    QVERIFY2(result.contains("error"), result.dump().c_str());
    QVERIFY(bridge.loadCalls.isEmpty());
}

void McpWorkspaceTools_Test::loadWorkspace_runningFunctions_refused()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("show.qxw");
    QVERIFY(writeWorkspaceFile(path));

    FakeWorkspaceBridge bridge;

    startSomething(m_doc);
    QTRY_VERIFY(m_doc->masterTimer()->runningFunctions() > 0);

    Json refused = invoke(m_doc, &bridge, "load_workspace", Json{{"path", path.toStdString()}});

    // Loading stops the MasterTimer and resets universes, blacking out the rig.
    QVERIFY2(refused.contains("error"), refused.dump().c_str());
    QCOMPARE(refused.value("runningFunctions", 0), 1);
    QVERIFY(bridge.loadCalls.isEmpty());

    Json forced = invoke(m_doc, &bridge, "load_workspace",
                         Json{{"path", path.toStdString()}, {"interruptLiveOutput", true}});
    QCOMPARE(forced.value("status", std::string()), std::string("loaded"));

    m_doc->masterTimer()->stop();
}

void McpWorkspaceTools_Test::saveWorkspace_existingOtherFile_needsOverwrite()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString other = dir.filePath("Gig-B.qxw");
    QFile file(other);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("someone else's project");
    file.close();

    FakeWorkspaceBridge bridge;
    bridge.fileName = dir.filePath("Gig-A.qxw");

    Json refused = invoke(m_doc, &bridge, "save_workspace", Json{{"path", other.toStdString()}});

    // App::saveXML removes the target before renaming over it, so this would
    // have destroyed an unrelated project.
    QVERIFY2(refused.contains("error"), refused.dump().c_str());
    QVERIFY(bridge.saveCalls.isEmpty());

    Json forced = invoke(m_doc, &bridge, "save_workspace",
                         Json{{"path", other.toStdString()}, {"overwrite", true}});
    QCOMPARE(forced.value("status", std::string()), std::string("saved"));
    QCOMPARE(bridge.saveCalls, QStringList{other});
}

void McpWorkspaceTools_Test::saveWorkspace_ownFile_noOverwriteNeeded()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString own = dir.filePath("Gig-A.qxw");
    QFile file(own);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("previous save");
    file.close();

    FakeWorkspaceBridge bridge;
    bridge.fileName = own;

    // Re-saving the current project is the normal case and must not need a flag.
    Json result = invoke(m_doc, &bridge, "save_workspace", Json{{"path", own.toStdString()}});

    QCOMPARE(result.value("status", std::string()), std::string("saved"));
    QCOMPARE(bridge.saveCalls, QStringList{own});
}

void McpWorkspaceTools_Test::saveWorkspace_relativePath_rejected()
{
    FakeWorkspaceBridge bridge;

    // A relative path would silently land in the app's working directory.
    Json result = invoke(m_doc, &bridge, "save_workspace", Json{{"path", "myshow"}});

    QVERIFY2(result.contains("error"), result.dump().c_str());
    QVERIFY(bridge.saveCalls.isEmpty());
}

void McpWorkspaceTools_Test::saveWorkspace_missingDirectory_rejected()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    FakeWorkspaceBridge bridge;

    // App::saveWorkspace repoints Doc's workspace path before writing, so a
    // doomed save would leave asset paths pointing at a directory that is not
    // there.
    Json result = invoke(m_doc, &bridge, "save_workspace",
                         Json{{"path", dir.filePath("nope/deeper/x.qxw").toStdString()}});

    QVERIFY2(result.contains("error"), result.dump().c_str());
    QVERIFY(bridge.saveCalls.isEmpty());
}

void McpWorkspaceTools_Test::newWorkspace_runningFunctions_refused()
{
    FakeWorkspaceBridge bridge;

    startSomething(m_doc);
    QTRY_VERIFY(m_doc->masterTimer()->runningFunctions() > 0);

    Json result = invoke(m_doc, &bridge, "new_workspace", Json::object());

    QVERIFY2(result.contains("error"), result.dump().c_str());
    QCOMPARE(bridge.newCalls, 0);

    m_doc->masterTimer()->stop();
}

void McpWorkspaceTools_Test::guardFlags_nonBoolean_rejected_data()
{
    QTest::addColumn<QString>("args");

    QTest::newRow("discardUnsaved string") << QStringLiteral(R"({"discardUnsaved": "true"})");
    QTest::newRow("discardUnsaved number") << QStringLiteral(R"({"discardUnsaved": 1})");
    QTest::newRow("interrupt string")      << QStringLiteral(R"({"interruptLiveOutput": "yes"})");
}

void McpWorkspaceTools_Test::guardFlags_nonBoolean_rejected()
{
    QFETCH(QString, args);

    FakeWorkspaceBridge bridge;
    // Dirty, so both guards are actually consulted.
    dirty(m_doc);

    Json result = invoke(m_doc, &bridge, "new_workspace", Json::parse(args.toStdString()));

    QVERIFY2(result.contains("error"), result.dump().c_str());
    QCOMPARE(bridge.newCalls, 0);
}

QTEST_MAIN(McpWorkspaceTools_Test)
