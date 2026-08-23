/*
  Q Light Controller Plus
  workspace_tools.cpp

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

#include "tool_registry.h"
#include "workspacebridge.h"
#include "doc.h"
#include "mastertimer.h"
#include "qlcfile.h"

#include <QDir>
#include <QFileInfo>
#include <QString>
#include <QUrl>
#include <QXmlStreamReader>

#include <fastmcpp/tools/manager.hpp>
#include <fastmcpp/tools/tool.hpp>

namespace {

using Json = nlohmann::json;

// App::loadXML compares the document DTD against this before accepting a file.
const QString kWorkspaceDtd = QStringLiteral("Workspace");

/** Strip a file: URL down to a plain local path, as App does. */
QString toLocalPath(const QString &path)
{
    return path.startsWith("file:") ? QUrl(path).toLocalFile() : path;
}

/**
 * App::loadWorkspace clears the document *before* it discovers whether the file
 * parses as a workspace, and cannot roll back — a bad path costs the whole
 * project. Run App's own acceptance check here, while the project is still
 * intact, and refuse anything it would reject.
 */
std::optional<std::string> workspaceFileGuard(const QString &localPath)
{
    const QFileInfo info(localPath);
    if (!info.exists())
        return Json({{"error", "no such file"}, {"path", localPath.toStdString()}}).dump();
    if (info.isDir())
        return Json({{"error", "path is a directory"}, {"path", localPath.toStdString()}}).dump();
    if (!info.isReadable())
        return Json({{"error", "file is not readable"}, {"path", localPath.toStdString()}}).dump();

    QXmlStreamReader *reader = QLCFile::getXMLReader(localPath);
    if (reader == NULL || reader->device() == NULL || reader->hasError())
    {
        if (reader != NULL)
            QLCFile::releaseXMLReader(reader);
        return Json({{"error", "file could not be read as XML"},
                     {"path", localPath.toStdString()}}).dump();
    }

    while (!reader->atEnd())
    {
        if (reader->readNext() == QXmlStreamReader::DTD)
            break;
    }
    const bool isWorkspace = !reader->hasError() && reader->dtdName() == kWorkspaceDtd;
    QLCFile::releaseXMLReader(reader);

    if (!isWorkspace)
        return Json({{"error", "not a QLC+ workspace file — loading it would clear the current "
                               "project without being able to restore it"},
                     {"path", localPath.toStdString()}}).dump();

    return std::nullopt;
}

/**
 * Clearing or replacing the project stops the MasterTimer and resets the
 * universes, so anything on stage goes dark mid-cue. The MCP surface does not
 * actuate a show, so make interrupting live output an explicit choice.
 */
std::optional<std::string> liveOutputGuard(Doc *doc, const Json &args)
{
    MasterTimer *timer = doc->masterTimer();
    const int running = timer ? timer->runningFunctions() : 0;
    if (running == 0)
        return std::nullopt;
    if (args.value("interruptLiveOutput", false))
        return std::nullopt;

    return Json({
        {"error", "functions are running — this would black out live output. Stop them first, "
                  "or pass interruptLiveOutput:true"},
        {"runningFunctions", running}
    }).dump();
}

/**
 * Both load and new throw away whatever is in memory. Refuse when the project
 * has unsaved edits unless the caller says so explicitly — an agent cannot see
 * the "unsaved changes" dialog a human would get here.
 */
std::optional<std::string> unsavedGuard(Doc *doc, const Json &args, const char *action)
{
    if (args.contains("discardUnsaved") && !args.at("discardUnsaved").is_boolean())
        return Json({{"error", "discardUnsaved must be a boolean"}}).dump();
    if (!doc->isModified())
        return std::nullopt;
    if (args.value("discardUnsaved", false))
        return std::nullopt;

    return Json({
        {"error", std::string("workspace has unsaved changes — save_workspace first, or pass "
                              "discardUnsaved:true to ") + action + " anyway"},
        {"modified", true}
    }).dump();
}

}

void registerWorkspaceTools(fastmcpp::tools::ToolManager &tm, Doc *doc, WorkspaceBridge *wsBridge)
{
    using Tool = fastmcpp::tools::Tool;

    if (!wsBridge) return;

    // query_workspace_file — where the project lives and whether it is dirty
    tm.register_tool(Tool(
        "query_workspace_file",
        Json{{"type", "object"}, {"properties", Json::object()}},
        Json{},
        [doc, wsBridge](const Json &) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            const QString path = wsBridge->currentFileName();
            Json result;
            result["path"] = path.toStdString();
            result["saved"] = !path.isEmpty();
            result["modified"] = doc->isModified();
            if (!path.isEmpty())
                result["name"] = QFileInfo(path).fileName().toStdString();
            return result.dump();
            });
        },
        std::nullopt,
        std::string("Report the current project file path and whether it has unsaved changes. "
                     "An empty path means the workspace has never been saved."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotReadOnly));

    // save_workspace — write the project to disk
    tm.register_tool(Tool(
        "save_workspace",
        Json{{"type", "object"}, {"properties", {
            {"path", {{"type", "string"}, {"description",
                "Absolute path to save to. Omit to save over the current project file. "
                "The .qxw suffix is added when missing."}}},
            {"overwrite", {{"type", "boolean"}, {"description",
                "Required to replace an existing file other than the current project. Default false."}}}
        }}},
        Json{},
        [doc, wsBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto err = validateFields(args, {"path", "overwrite"});
            if (!err.empty()) return err;
            if (args.contains("overwrite") && !args.at("overwrite").is_boolean())
                return Json({{"error", "overwrite must be a boolean"}}).dump();

            QString path;
            if (args.contains("path"))
            {
                if (!args.at("path").is_string())
                    return Json({{"error", "path must be a string"}}).dump();
                path = toLocalPath(QString::fromStdString(args.at("path").get<std::string>()).trimmed());
                if (path.isEmpty())
                    return Json({{"error", "path must not be empty"}}).dump();
                if (!QDir::isAbsolutePath(path))
                    return Json({{"error", "path must be absolute — a relative path would land in "
                                           "the application's working directory"},
                                 {"path", path.toStdString()}}).dump();
            }
            else
            {
                path = wsBridge->currentFileName();
                if (path.isEmpty())
                    return Json({{"error", "workspace has never been saved — provide a path"}}).dump();
            }

            // App::saveXML removes the target before renaming the new file over
            // it, so an unrelated project at this path would be destroyed.
            const QString target = path.endsWith(QStringLiteral(".qxw")) ? path : path + ".qxw";
            const QString current = wsBridge->currentFileName();
            if (QFileInfo::exists(target) && target != current && !args.value("overwrite", false))
            {
                return Json({{"error", "a different project already exists at this path — pass "
                                       "overwrite:true to replace it"},
                             {"path", target.toStdString()}}).dump();
            }

            // App::saveWorkspace repoints Doc's workspace path before it writes,
            // so a save into a missing directory leaves asset paths broken.
            const QDir parent = QFileInfo(target).absoluteDir();
            if (!parent.exists())
                return Json({{"error", "target directory does not exist"},
                             {"path", parent.absolutePath().toStdString()}}).dump();

            if (!wsBridge->saveWorkspace(path))
                return Json({{"error", "could not save workspace"}, {"path", path.toStdString()}}).dump();

            // saveWorkspace appends the .qxw suffix when it is missing, so report
            // what the app ended up with rather than what was asked for.
            const QString saved = wsBridge->currentFileName();
            return Json({
                {"status", "saved"},
                {"path", (saved.isEmpty() ? path : saved).toStdString()},
                {"modified", doc->isModified()}
            }).dump();
            });
        },
        std::nullopt,
        std::string("Save the current project to disk. Without a path it overwrites the current "
                     "project file, and fails if the workspace was never saved. Replacing a "
                     "different existing file requires overwrite:true."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotIdempotent));

    // load_workspace — replace everything with a project from disk
    tm.register_tool(Tool(
        "load_workspace",
        Json{{"type", "object"}, {"properties", {
            {"path", {{"type", "string"}, {"description", "Absolute path of the .qxw project to open"}}},
            {"discardUnsaved", {{"type", "boolean"}, {"description",
                "Required to proceed when the current workspace has unsaved changes. Default false."}}},
            {"interruptLiveOutput", {{"type", "boolean"}, {"description",
                "Required to proceed while functions are running. Default false."}}}
        }}, {"required", {"path"}}},
        Json{},
        [doc, wsBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto err = validateFields(args, {"path", "discardUnsaved", "interruptLiveOutput"});
            if (!err.empty()) return err;
            if (!args.contains("path") || !args.at("path").is_string())
                return Json({{"error", "path is required and must be a string"}}).dump();

            const QString localPath =
                toLocalPath(QString::fromStdString(args.at("path").get<std::string>()).trimmed());
            if (localPath.isEmpty())
                return Json({{"error", "path must not be empty"}}).dump();

            if (auto guard = workspaceFileGuard(localPath))
                return *guard;
            if (auto guard = unsavedGuard(doc, args, "load"))
                return *guard;
            if (auto guard = liveOutputGuard(doc, args))
                return *guard;

            if (!wsBridge->loadWorkspace(localPath))
                return Json({{"error", "could not load workspace"},
                             {"path", localPath.toStdString()}}).dump();

            return Json({
                {"status", "loaded"},
                {"path", wsBridge->currentFileName().toStdString()}
            }).dump();
            });
        },
        std::nullopt,
        std::string("Open a .qxw project, replacing everything currently loaded. The file is "
                     "validated as a workspace first, because loading clears the current project "
                     "before parsing and cannot roll back. Refuses on unsaved changes unless "
                     "discardUnsaved is true, and while functions are running unless "
                     "interruptLiveOutput is true. The loaded project's startup function, if it "
                     "has one, begins playing."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotDestructive));

    // new_workspace — start from nothing
    tm.register_tool(Tool(
        "new_workspace",
        Json{{"type", "object"}, {"properties", {
            {"discardUnsaved", {{"type", "boolean"}, {"description",
                "Required to proceed when the current workspace has unsaved changes. Default false."}}},
            {"interruptLiveOutput", {{"type", "boolean"}, {"description",
                "Required to proceed while functions are running. Default false."}}}
        }}},
        Json{},
        [doc, wsBridge](const Json &args) -> Json {
            return execOnMainThread(doc, [&]() -> Json {
            auto err = validateFields(args, {"discardUnsaved", "interruptLiveOutput"});
            if (!err.empty()) return err;

            if (auto guard = unsavedGuard(doc, args, "reset"))
                return *guard;
            if (auto guard = liveOutputGuard(doc, args))
                return *guard;

            if (!wsBridge->newWorkspace())
                return Json({{"error", "could not reset workspace"}}).dump();

            return Json({{"status", "reset"}}).dump();
            });
        },
        std::nullopt,
        std::string("Discard the current project and start an empty workspace. Refuses on unsaved "
                     "changes unless discardUnsaved is true, and while functions are running "
                     "unless interruptLiveOutput is true."),
        std::nullopt
    )
    .set_annotations(mcp::kAnnotDestructive));
}
