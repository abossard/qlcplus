/*
  Q Light Controller Plus
  webaccess-qml.cpp

  Copyright (c) Massimo Callegari

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

#include <QDebug>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QColor>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTimer>
#include <qmath.h>

#include "webaccess-qml.h"
#include "webaccessauth.h"
#include "webaccesssimpledesk.h"
#include "webaccessnetwork.h"
#include "commonjscss.h"
#include "qlcfile.h"
#include "qlcconfig.h"

#include "virtualconsole.h"
#include "vcaudiotriggers.h"
#include "vcanimation.h"
#include "vcspeeddial.h"
#include "vccuelist.h"
#include "vcbutton.h"
#include "vcslider.h"
#include "vcframe.h"
#include "vclabel.h"
#include "vcclock.h"
#include "vcxypad.h"
#include "vcpage.h"

#include "doc.h"
#include "fixture.h"
#include "qlcchannel.h"
#include "qlccapability.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
#include "qlcphysical.h"

#include "function.h"
#include "chaser.h"
#include "doc.h"
#include "fixture.h"
#include "qlcchannel.h"
#include "qlccapability.h"
#include "listmodel.h"
#include "simpledesk.h"
#include "ioplugincache.h"
#include "qlcioplugin.h"
#include "inputoutputmap.h"
#include "inputpatch.h"
#include "outputpatch.h"

#include "qhttprequest.h"
#include "qhttpresponse.h"
#include "qhttpconnection.h"


static QJsonObject fontToJson(const QFont &font)
{
    QJsonObject obj;
    obj["family"] = font.family();
    obj["pixelSize"] = font.pixelSize();
    obj["pointSize"] = font.pointSize();
    obj["bold"] = font.bold();
    obj["italic"] = font.italic();
    obj["weight"] = font.weight();
    return obj;
}

static QJsonObject rectToJson(const QRectF &rect)
{
    QJsonObject obj;
    obj["x"] = rect.x();
    obj["y"] = rect.y();
    obj["w"] = rect.width();
    obj["h"] = rect.height();
    return obj;
}

static QString colorToString(const QColor &color)
{
    if (!color.isValid())
        return QString();
    return color.name();
}

static QString mimeTypeForPath(const QString &path);

static void setFramePageCompat(VCFrame *frame, int page)
{
    if (frame == nullptr)
        return;

    if (QMetaObject::invokeMethod(frame, "gotoPage", Q_ARG(int, page)))
        return;

    if (QMetaObject::invokeMethod(frame, "slotSetPage", Q_ARG(int, page)))
        return;

    frame->setProperty("currentPage", page);
}

static QString resourcePathToWebUrl(const QString &path)
{
    if (path.startsWith(":/"))
        return QString("/qrc/%1").arg(path.mid(2));
    if (path.startsWith("qrc:/"))
        return QString("/qrc/%1").arg(path.mid(5));
    if (QFile::exists(path))
    {
        QFile resFile(path);
        if (resFile.open(QIODevice::ReadOnly))
        {
            const QByteArray content = resFile.readAll();
            resFile.close();
            if (content.isEmpty() == false)
            {
                return QString("data:%1;base64,%2")
                        .arg(mimeTypeForPath(path))
                        .arg(QString::fromLatin1(content.toBase64()));
            }
        }
    }
    return path;
}

static QColor variantToColor(const QVariant &value)
{
    QColor color = value.value<QColor>();
    if (color.isValid())
        return color;
    return QColor(value.toString());
}

static QJsonObject clickAndGoCapabilityToJson(const QLCCapability *cap)
{
    QJsonObject obj;
    if (cap == nullptr)
        return obj;

    obj["name"] = cap->name();
    obj["min"] = int(cap->min());
    obj["max"] = int(cap->max());
    obj["value"] = int(cap->middle());
    obj["presetType"] = int(cap->presetType());

    switch (cap->presetType())
    {
        case QLCCapability::SingleColor:
        {
            const QColor color1 = variantToColor(cap->resource(0));
            obj["color1"] = colorToString(color1);
        }
        break;
        case QLCCapability::DoubleColor:
        {
            const QColor color1 = variantToColor(cap->resource(0));
            const QColor color2 = variantToColor(cap->resource(1));
            obj["color1"] = colorToString(color1);
            obj["color2"] = colorToString(color2);
        }
        break;
        case QLCCapability::Picture:
        {
            const QString resourcePath = cap->resource(0).toString();
            obj["resource"] = resourcePathToWebUrl(resourcePath);
        }
        break;
        default:
        break;
    }

    return obj;
}

static QJsonArray sliderClickAndGoPresetsToJson(const VCSlider *slider, const Doc *doc)
{
    QJsonArray array;
    if (slider == nullptr || doc == nullptr || slider->clickAndGoType() != VCSlider::CnGPreset)
        return array;

    const QVariantList channels = const_cast<VCSlider*>(slider)->clickAndGoPresetsList();
    if (channels.isEmpty())
        return array;

    const QVariantMap firstChannel = channels.first().toMap();
    const quint32 fixtureID = firstChannel.value("fixtureID").toUInt();
    const int channelIdx = firstChannel.value("channelIdx").toInt();

    Fixture *fixture = doc->fixture(fixtureID);
    if (fixture == nullptr)
        return array;

    const QLCChannel *channel = fixture->channel(channelIdx);
    if (channel == nullptr)
        return array;

    for (const QLCCapability *cap : channel->capabilities())
        array.append(clickAndGoCapabilityToJson(cap));

    return array;
}

static QJsonObject loadUiStyleJson()
{
    QDir userConfDir = QLCFile::userDirectory(QString(USERQLCPLUSDIR),
                                              QString(USERQLCPLUSDIR),
                                              QStringList());
    const QString stylePath = userConfDir.absolutePath()
            + QDir::separator()
            + QStringLiteral("qlcplusUiStyle.json");
    QFile jsonFile(stylePath);
    if (jsonFile.exists() == false || jsonFile.open(QIODevice::ReadOnly) == false)
        return QJsonObject();

    QJsonParseError parseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonFile.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || jsonDoc.isObject() == false)
        return QJsonObject();

    return jsonDoc.object();
}

static QString sliderDisplayValue(VCSlider *slider)
{
    if (slider == nullptr)
        return QString();

    if (slider->valueDisplayStyle() == VCSlider::DMXValue)
        return QString::number(slider->value());

    int p = qFloor(((double(slider->value()) / double(UCHAR_MAX)) * double(100)) + 0.5);
    return QString::number(p);
}

static QString mimeTypeForPath(const QString &path)
{
    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == "html" || ext == "htm") return "text/html";
    if (ext == "otf") return "font/otf";
    if (ext == "ttf") return "font/ttf";
    if (ext == "woff") return "font/woff";
    if (ext == "woff2") return "font/woff2";
    if (ext == "svg") return "image/svg+xml";
    if (ext == "png") return "image/png";
    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "gif") return "image/gif";
    if (ext == "css") return "text/css";
    if (ext == "js") return "text/javascript";
    if (ext == "json") return "application/json";
    return "application/octet-stream";
}

static int clockScheduledDaysMask(const VCClock *clock)
{
    if (clock == nullptr || clock->clockType() != VCClock::Clock)
        return 0;

    int mask = 0;
    const QList<VCClockSchedule*> schedules = clock->schedules();
    for (VCClockSchedule *sch : schedules)
    {
        if (sch == nullptr)
            continue;

        const int weekMask = sch->weekFlags() & 0x7F;
        // Show day initials only for specific-day schedules.
        if (weekMask != 0 && weekMask != 0x7F)
            mask |= weekMask;
    }

    return mask;
}

static bool isWidgetVisibleForWeb(const VCWidget *widget, const VirtualConsole *vc)
{
    if (widget == nullptr || vc == nullptr)
        return false;
    if (widget->isVisible() == false)
        return false;

    const QObject *obj = widget;
    const VCPage *pageParent = nullptr;
    while (obj != nullptr)
    {
        const VCWidget *childWidget = qobject_cast<const VCWidget *>(obj);
        const VCFrame *frameParent = qobject_cast<const VCFrame *>(obj->parent());
        if (childWidget != nullptr && frameParent != nullptr && frameParent->multiPageMode())
        {
            if (childWidget->page() != frameParent->currentPage())
                return false;
        }

        const VCPage *page = qobject_cast<const VCPage *>(obj);
        if (page != nullptr)
            pageParent = page;

        obj = obj->parent();
    }

    if (pageParent != nullptr)
        return pageParent == vc->page(vc->selectedPage());

    return true;
}
/*
static void logWidgetTree(VCWidget *widget, int depth)
{
    if (widget == nullptr)
        return;

    const QRectF geom = widget->geometry();
    const QString indent(depth * 2, ' ');
    qDebug().noquote() << indent
        + QString("%1 id=%2 page=%3 geom=%4,%5 %6x%7")
              .arg(VCWidget::typeToString(widget->type()))
              .arg(widget->id())
              .arg(widget->page())
              .arg(geom.x())
              .arg(geom.y())
              .arg(geom.width())
              .arg(geom.height());

    VCFrame *frame = qobject_cast<VCFrame *>(widget);
    if (frame == nullptr)
        return;

    const QList<VCWidget *> children = frame->children(false);
    for (VCWidget *child : children)
        logWidgetTree(child, depth + 1);
}
*/
static QString getSimpleDeskQmlHtml(const Doc *doc, const SimpleDesk *sd)
{
    if (doc == nullptr || sd == nullptr)
        return QString();

    int uni = sd->getCurrentUniverseIndex() + 1;
    if (uni < 1)
        uni = 1;
    int page = sd->getCurrentPage();

    QString JScode = "<script src=\"simpledesk-v5.js\"></script>\n";
    JScode += "<script>\n";
    JScode += "var currentUniverse = " + QString::number(uni) + ";\n";
    JScode += "var currentPage = " + QString::number(page) + ";\n";
    JScode += "var channelsPerPage = " + QString::number(sd->getSlidersNumber()) + ";\n";
    JScode += "</script>\n";

    QString CSScode = "<link rel=\"stylesheet\" type=\"text/css\" media=\"screen\" href=\"webaccess-v5.css\">\n";
    CSScode += "<link rel=\"stylesheet\" type=\"text/css\" media=\"screen\" href=\"simpledesk-v5.css\">\n";

    QString bodyHTML = "<div id=\"app\">\n"
                       "<header class=\"topbar\">\n"
                       "<div class=\"brand\">\n"
                       "<div class=\"brand-title\">" + QObject::tr("Simple Desk") + "</div>\n"
                       "<div class=\"brand-sub\">" + QString(APPNAME) + " " + QString(APPVERSION) + "</div>\n"
                       "</div>\n"
                       "<div class=\"sd-topbar-center\">\n"
                       "<div class=\"sd-section\">\n"
                       "<div class=\"sd-label\">" + QObject::tr("Universe") + "</div>\n"
                       "<select class=\"sd-select\" id=\"universeSelect\">\n";

    QStringList uniList = doc->inputOutputMap()->universeNames();
    for (int i = 0; i < uniList.count(); i++)
    {
        QString selected = (i + 1 == uni) ? " selected" : "";
        bodyHTML += "<option value=\"" + QString::number(i) + "\"" + selected + ">"
                + uniList.at(i) + "</option>\n";
    }

    bodyHTML += "</select>\n"
                "</div>\n"
                "<button class=\"nav-btn\" id=\"resetUniverseBtn\" type=\"button\">"
                + QObject::tr("Reset universe") + "</button>\n"
                "<div class=\"sd-section\">\n"
                "<div class=\"sd-label\">" + QObject::tr("Faders") + "</div>\n"
                "<select class=\"sd-select\" id=\"fadersSelect\">\n"
                "<option value=\"8\">8</option>\n"
                "<option value=\"16\">16</option>\n"
                "<option value=\"24\">24</option>\n"
                "<option value=\"32\">32</option>\n"
                "<option value=\"48\">48</option>\n"
                "<option value=\"64\">64</option>\n"
                "</select>\n"
                "</div>\n"
                "<div class=\"sd-section\">\n"
                "<div class=\"sd-label\">" + QObject::tr("Page") + "</div>\n"
                "<button class=\"nav-btn\" id=\"pagePrev\" type=\"button\">&#x2039;</button>\n"
                "<div class=\"sd-page-display\" id=\"pageDisplay\">" + QString::number(page) + "</div>\n"
                "<button class=\"nav-btn\" id=\"pageNext\" type=\"button\">&#x203a;</button>\n"
                "</div>\n"
                "</div>\n"
                "<div class=\"topbar-right\">\n"
                "<div class=\"actions\">\n"
                "<a class=\"nav-btn\" href=\"/\">" + QObject::tr("Back") + "</a>\n"
                "<a class=\"nav-btn\" href=\"/keypad.html\">DMX Keypad</a>\n"
                "</div>\n"
                "<div class=\"status disconnected\" id=\"wsStatus\" aria-label=\"Disconnected\" title=\"Disconnected\"></div>\n"
                "</div>\n"
                "</header>\n"
                "<main class=\"sd-stage\">\n"
                "<div class=\"sd-sliders\" id=\"slidersContainer\"></div>\n"
                "</main>\n"
                "</div>\n";

    return QString(HTML_HEADER) + JScode + CSScode + "</head>\n<body>\n" + bodyHTML + "</body>\n</html>";
}

WebAccessQml::WebAccessQml(Doc *doc, VirtualConsole *vcInstance, SimpleDesk *sdInstance,
                           int portNumber, bool enableAuth, QString passwdFile, QObject *parent)
    : WebAccessBase(doc, vcInstance, sdInstance, portNumber, enableAuth, passwdFile, parent)
{
    connect(m_doc, SIGNAL(loaded()),
            this, SLOT(slotDocLoaded()));

    connect(m_doc->inputOutputMap(), SIGNAL(grandMasterValueChanged(uchar)),
            this, SLOT(slotGrandMasterValueChanged(uchar)));

    connect(m_doc->inputOutputMap(), &InputOutputMap::universeWritten,
            this, &WebAccessQml::slotUniverseWritten, Qt::QueuedConnection);

    connect(m_vc, SIGNAL(selectedPageChanged(int)),
            this, SLOT(slotSelectedPageChanged(int)));
}

WebAccessQml::~WebAccessQml()
{
}

void WebAccessQml::slotHandleHTTPRequest(QHttpRequest *req, QHttpResponse *resp)
{
    WebAccessUser user;

    if (!authenticateRequest(req, resp, user))
        return;

    QString reqUrl = req->url().path();
    QString content;

    qDebug() << Q_FUNC_INFO << req->methodString() << req->url();

    if (reqUrl == "/vc.json")
    {
        if (!requireAuthLevel(resp, user, VC_ONLY_LEVEL))
            return;
        QByteArray json = getVCJson();
        resp->setHeader("Content-Type", "application/json");
        resp->setHeader("Content-Length", QString::number(json.size()));
        resp->writeHead(200);
        resp->end(json);
        return;
    }
    // Modern VC web app (vc-next): serve built files from webaccess/vc-next/dist/
    else if (reqUrl == "/vc" || reqUrl == "/vc/")
    {
        if (serveVCNextFile(resp, "/index.html"))
            return;
        sendNotFound(resp);
        return;
    }
    else if (reqUrl.startsWith("/vc/"))
    {
        QString filePath = reqUrl.mid(3); // strip "/vc" prefix, keep leading "/"
        if (serveVCNextFile(resp, filePath))
            return;
        // SPA fallback: serve index.html for unmatched routes
        if (serveVCNextFile(resp, "/index.html"))
            return;
        sendNotFound(resp);
        return;
    }
    else if (reqUrl == "/api/fixtures")
    {
        QByteArray json = buildFixturesJson();
        resp->setHeader("Content-Type", "application/json");
        resp->setHeader("Content-Length", QString::number(json.size()));
        resp->writeHead(200);
        resp->end(json);
        return;
    }
    else if (reqUrl == "/api/channels")
    {
        QList<quint32> ids;
        QString idsParam = req->url().query(QUrl::FullyDecoded);
        // Parse "fixtureIDs=0,1,2" from query string
        QStringList pairs = idsParam.split('&', Qt::SkipEmptyParts);
        for (const QString &pair : pairs)
        {
            if (pair.startsWith("fixtureIDs="))
            {
                QString val = pair.mid(QString("fixtureIDs=").length());
                for (const QString &part : val.split(',', Qt::SkipEmptyParts))
                {
                    bool ok = false;
                    quint32 id = part.trimmed().toUInt(&ok);
                    if (ok) ids.append(id);
                }
            }
        }
        QByteArray json = buildChannelsJson(ids);
        resp->setHeader("Content-Type", "application/json");
        resp->setHeader("Content-Length", QString::number(json.size()));
        resp->writeHead(200);
        resp->end(json);
        return;
    }
    else if (reqUrl.startsWith("/qrc/"))
    {
        QString qrcPath = ":/" + reqUrl.mid(5);
        if (sendFile(resp, qrcPath, mimeTypeForPath(qrcPath)))
            return;
    }
    else if (reqUrl == "/simpleDesk")
    {
        if (!requireAuthLevel(resp, user, SIMPLE_DESK_AND_VC_LEVEL))
            return;
        content = getSimpleDeskQmlHtml(m_doc, m_sd);
        sendHtmlResponse(resp, content);
        return;
    }
    else if (reqUrl == "/diagnostics.json" || reqUrl == "/os2l.json")
    {
        // Unified diagnostics JSON — gated behind diagnostics enabled
        // Check if ANY plugin has diagnostics enabled
        IOPluginCache *cache = m_doc->ioPluginCache();
        bool anyEnabled = false;
        for (QLCIOPlugin *p : cache->plugins())
        {
            if (p->isDiagnosticsEnabled())
            {
                anyEnabled = true;
                break;
            }
        }

        QJsonObject root;
        root["enabled"] = anyEnabled;

        QJsonObject pluginsObj;
        for (QLCIOPlugin *p : cache->plugins())
        {
            QByteArray diag = p->pluginDiagnostics();
            if (!diag.isEmpty())
            {
                QJsonDocument pluginDoc = QJsonDocument::fromJson(diag);
                if (!pluginDoc.isNull())
                    pluginsObj[p->name()] = pluginDoc.object();
            }
            else
            {
                // Provide basic info even if no diagnostics override
                QJsonObject basic;
                basic["capabilities"] = p->capabilities();
                basic["inputs"] = QJsonArray::fromStringList(p->inputs());
                basic["outputs"] = QJsonArray::fromStringList(p->outputs());
                basic["diagnosticsEnabled"] = p->isDiagnosticsEnabled();
                pluginsObj[p->name()] = basic;
            }
        }
        root["plugins"] = pluginsObj;

        // Universe mapping
        QJsonObject universesObj;
        InputOutputMap *ioMap = m_doc->inputOutputMap();
        for (quint32 uni = 0; uni < ioMap->universesCount(); uni++)
        {
            QJsonObject uniObj;
            uniObj["name"] = ioMap->getUniverseNameByIndex(uni);
            InputPatch *inPatch = ioMap->inputPatch(uni);
            OutputPatch *outPatch = ioMap->outputPatch(uni);
            if (inPatch)
                uniObj["input"] = inPatch->pluginName();
            if (outPatch)
                uniObj["output"] = outPatch->pluginName();
            universesObj[QString::number(uni)] = uniObj;
        }
        root["universes"] = universesObj;

        QByteArray json = QJsonDocument(root).toJson(QJsonDocument::Compact);
        resp->setHeader("Content-Type", "application/json");
        resp->setHeader("Content-Length", QString::number(json.size()));
        resp->writeHead(200);
        resp->end(json);
        return;
    }
    else if (reqUrl == "/os2l" || reqUrl == "/diagnostics")
    {
        if (serveWebFile(resp, "/diagnostics.html", "text/html"))
            return;
        // Fallback to old name
        if (serveWebFile(resp, "/os2l-diag.html", "text/html"))
            return;
        sendNotFound(resp);
        return;
    }
    else if (reqUrl == "/debug-mode.json")
    {
        // Lightweight check for whether debug mode is active (used by the menu link)
        bool debugMode = QCoreApplication::instance() &&
                         QCoreApplication::instance()->property("debugMode").toBool();
        QByteArray json = debugMode ? QByteArray("{\"debug\":true}") : QByteArray("{\"debug\":false}");
        resp->setHeader("Content-Type", "application/json");
        resp->setHeader("Content-Length", QString::number(json.size()));
        resp->writeHead(200);
        resp->end(json);
        return;
    }
    else if (reqUrl == "/diagnostics/enable" || reqUrl == "/diagnostics/disable")
    {
        bool enable = (reqUrl == "/diagnostics/enable");
        IOPluginCache *cache = m_doc->ioPluginCache();
        for (QLCIOPlugin *p : cache->plugins())
            p->setDiagnosticsEnabled(enable);

        QByteArray json = enable ? QByteArray("{\"enabled\":true}") : QByteArray("{\"enabled\":false}");
        resp->setHeader("Content-Type", "application/json");
        resp->setHeader("Content-Length", QString::number(json.size()));
        resp->writeHead(200);
        resp->end(json);
        return;
    }
    CommonRequestResult commonResult = handleCommonHTTPRequest(req, resp, user, reqUrl, content);
    if (commonResult == CommonRequestResult::Handled)
        return;
    if (commonResult == CommonRequestResult::ContentReady)
    {
        sendHtmlResponse(resp, content);
        return;
    }

    if (serveWebFile(resp, "/webaccess-v5.html", "text/html"))
        return;
    content = QString(HTML_HEADER) + "</head><body>Missing webaccess-v5.html</body></html>";
    sendHtmlResponse(resp, content);
}

void WebAccessQml::handleProjectLoad(const QByteArray &projectXml)
{
    emit loadProject(projectXml);
}

void WebAccessQml::slotHandleWebSocketRequest(QHttpConnection *conn, QString data)
{
    if (conn == nullptr)
        return;

    // Handle JSON messages (DMX_* protocol)
    if (data.startsWith('{'))
    {
        QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8());
        if (!doc.isNull() && doc.isObject())
        {
            handleDmxJson(conn, doc.object());
            return;
        }
    }

    WebAccessUser *user = static_cast<WebAccessUser*>(conn->userData);

    QStringList cmdList = data.split("|");
    if (cmdList.isEmpty())
        return;

    if (cmdList[0] == "QLC+CMD")
    {
        return;
    }
    else if (cmdList[0] == "VC_PAGE")
    {
        if (m_auth && user && user->level < VC_ONLY_LEVEL)
            return;
        if (cmdList.count() > 1)
            m_vc->setSelectedPage(cmdList[1].toInt());
        return;
    }
    if (handleCommonWebSocketCommand(conn, user, cmdList, "[webaccess-v5]", true))
        return;
    else if (cmdList[0] == "QLC+API")
    {
        if (m_auth && user && user->level < VC_ONLY_LEVEL)
            return;

        if (cmdList.count() < 2)
            return;

        QString apiCmd = cmdList[1];
        QString wsAPIMessage = QString("QLC+API|%1|").arg(apiCmd);

        if (apiCmd == "isProjectLoaded")
        {
            wsAPIMessage.append(m_pendingProjectLoaded ? "true" : "false");
        }
        else if (apiCmd == "getFunctionsNumber")
        {
            wsAPIMessage.append(QString::number(m_doc->functions().count()));
        }
        else if (apiCmd == "getFunctionsList")
        {
            foreach (Function *f, m_doc->functions())
                wsAPIMessage.append(QString("%1|%2|").arg(f->id()).arg(f->name()));
            wsAPIMessage.truncate(wsAPIMessage.length() - 1);
        }
        else if (apiCmd == "getFunctionType")
        {
            if (cmdList.count() < 3)
                return;

            quint32 fID = cmdList[2].toUInt();
            Function *f = m_doc->function(fID);
            if (f != nullptr)
                wsAPIMessage.append(f->typeString());
            else
                wsAPIMessage.append(Function::typeToString(Function::Undefined));
        }
        else if (apiCmd == "getFunctionStatus")
        {
            if (cmdList.count() < 3)
                return;

            quint32 fID = cmdList[2].toUInt();
            Function *f = m_doc->function(fID);
            if (f != nullptr)
                wsAPIMessage.append(f->isRunning() ? "Running" : "Stopped");
            else
                wsAPIMessage.append(Function::typeToString(Function::Undefined));
        }
        else if (apiCmd == "setFunctionStatus")
        {
            if (cmdList.count() < 4)
                return;

            quint32 fID = cmdList[2].toUInt();
            quint32 newStatus = cmdList[3].toUInt();
            Function *f = m_doc->function(fID);

            if (f != nullptr)
            {
                if (!f->isRunning() && newStatus)
                    f->start(m_doc->masterTimer(), FunctionParent::master());
                else if (f->isRunning() && !newStatus)
                    f->stop(FunctionParent::master());
            }
            return;
        }
        else if (apiCmd == "getWidgetsNumber")
        {
            QList<VCWidget *> widgets;
            for (int i = 0; i < m_vc->pagesCount(); i++)
                collectWidgets(m_vc->page(i), widgets, true);
            wsAPIMessage.append(QString::number(widgets.count()));
        }
        else if (apiCmd == "getWidgetsList")
        {
            QList<VCWidget *> widgets;
            for (int i = 0; i < m_vc->pagesCount(); i++)
                collectWidgets(m_vc->page(i), widgets, true);

            foreach (VCWidget *widget, widgets)
                wsAPIMessage.append(QString("%1|%2|").arg(widget->id()).arg(widget->caption()));
            wsAPIMessage.truncate(wsAPIMessage.length() - 1);
        }
        else if (apiCmd == "getWidgetType")
        {
            if (cmdList.count() < 3)
                return;

            quint32 wID = cmdList[2].toUInt();
            VCWidget *widget = m_vc->widget(wID);
            if (widget != nullptr)
                wsAPIMessage.append(QString("%1|%2").arg(wID).arg(VCWidget::typeToString(widget->type())));
            else
                wsAPIMessage.append(QString("%1|%2").arg(wID).arg(VCWidget::typeToString(VCWidget::UnknownWidget)));
        }
        else if (apiCmd == "getWidgetFunction")
        {
            if (cmdList.count() < 3)
                return;

            quint32 wID = cmdList[2].toUInt();
            VCWidget *widget = m_vc->widget(wID);

            wsAPIMessage.append(QString("%1|").arg(wID));

            quint32 fID = 0;

            if (widget != nullptr)
            {
                switch (widget->type())
                {
                    case VCWidget::ButtonWidget:
                    {
                        VCButton *button = qobject_cast<VCButton*>(widget);
                        if (button != nullptr && button->functionID() != Function::invalidId())
                            fID = button->functionID();
                    }
                    break;
                    case VCWidget::CueListWidget:
                    {
                        VCCueList *cue = qobject_cast<VCCueList*>(widget);
                        if (cue != nullptr && cue->chaserID() != Function::invalidId())
                            fID = cue->chaserID();
                    }
                    break;
                    case VCWidget::SliderWidget:
                    {
                        VCSlider *slider = qobject_cast<VCSlider*>(widget);
                        if (slider != nullptr && slider->controlledFunction() != Function::invalidId())
                            fID = slider->controlledFunction();
                    }
                    break;
                    default:
                        break;
                }
            }

            Function *f = (fID != 0) ? m_doc->function(fID) : nullptr;
            if (f != nullptr)
                wsAPIMessage.append(QString("%1|%2|%3").arg(f->id()).arg(f->typeString()).arg(f->name()));
            else
                wsAPIMessage.append(QString("0|%1|").arg(Function::typeToString(Function::Undefined)));
        }
        else if (apiCmd == "getWidgetStatus")
        {
            if (cmdList.count() < 3)
                return;

            quint32 wID = cmdList[2].toUInt();
            VCWidget *widget = m_vc->widget(wID);
            if (widget != nullptr)
            {
                wsAPIMessage.append(QString("%1|").arg(wID));

                switch (widget->type())
                {
                    case VCWidget::ButtonWidget:
                    {
                        VCButton *button = qobject_cast<VCButton*>(widget);
                        if (button->state() == VCButton::Active)
                            wsAPIMessage.append("255");
                        else if (button->state() == VCButton::Monitoring)
                            wsAPIMessage.append("127");
                        else
                            wsAPIMessage.append("0");
                    }
                    break;
                    case VCWidget::SliderWidget:
                    {
                        VCSlider *slider = qobject_cast<VCSlider*>(widget);
                        wsAPIMessage.append(QString::number(slider->value()));
                    }
                    break;
                    case VCWidget::CueListWidget:
                    {
                        VCCueList *cue = qobject_cast<VCCueList*>(widget);
                        if (cue->playbackStatus() == VCCueList::Playing)
                            wsAPIMessage.append(QString("PLAY|%1").arg(cue->playbackIndex()));
                        else
                            wsAPIMessage.append("STOP");
                    }
                    break;
                    case VCWidget::AnimationWidget:
                    {
                        VCAnimation *animation = qobject_cast<VCAnimation*>(widget);
                        wsAPIMessage.append(QString::number(animation->faderLevel()));
                    }
                    break;
                    default:
                        wsAPIMessage.append("0");
                    break;
                }
            }
        }
        else if (apiCmd == "getChannelsValues")
        {
            if (m_auth && user && user->level < SIMPLE_DESK_AND_VC_LEVEL)
                return;

            if (cmdList.count() < 4)
                return;

            quint32 universe = cmdList[2].toUInt() - 1;
            int startAddr = cmdList[3].toInt() - 1;
            int count = 1;
            if (cmdList.count() == 5)
                count = cmdList[4].toInt();

            wsAPIMessage.append(WebAccessSimpleDesk::getChannelsMessage(m_doc, m_sd, universe, startAddr, count));
        }
        else if (apiCmd == "sdResetChannel")
        {
            if (m_auth && user && user->level < SIMPLE_DESK_AND_VC_LEVEL)
                return;

            if (cmdList.count() < 3)
                return;

            quint32 chNum = cmdList[2].toUInt() - 1;
            m_sd->resetAbsoluteChannel(chNum);
            wsAPIMessage = "QLC+API|getChannelsValues|";
            wsAPIMessage.append(WebAccessSimpleDesk::getChannelsMessage(
                                m_doc, m_sd, m_sd->getCurrentUniverseIndex(),
                                (m_sd->getCurrentPage() - 1) * m_sd->getSlidersNumber(),
                                m_sd->getSlidersNumber()));
        }
        else if (apiCmd == "sdResetUniverse")
        {
            if (m_auth && user && user->level < SIMPLE_DESK_AND_VC_LEVEL)
                return;

            if (cmdList.count() < 3)
                return;

            quint32 universeIndex = cmdList[2].toUInt() - 1;
            m_sd->resetUniverse(universeIndex);
            wsAPIMessage = "QLC+API|getChannelsValues|";
            wsAPIMessage.append(WebAccessSimpleDesk::getChannelsMessage(
                                m_doc, m_sd, m_sd->getCurrentUniverseIndex(),
                                0, m_sd->getSlidersNumber()));
        }

        conn->webSocketWrite(wsAPIMessage);
        return;
    }
    else if (cmdList[0] == "CH")
    {
        if (m_auth && user && user->level < SIMPLE_DESK_AND_VC_LEVEL)
            return;

        if (cmdList.count() < 3)
            return;

        uint absAddress = cmdList[1].toInt() - 1;
        int value = cmdList[2].toInt();
        m_sd->setAbsoluteChannelValue(absAddress, uchar(value));
        return;
    }
    else if (cmdList[0] == "GM_VALUE")
    {
        uchar value = cmdList[1].toInt();
        m_doc->inputOutputMap()->setGrandMasterValue(value);
        return;
    }
    else if (cmdList[0] == "POLL")
        return;

    if (!data.contains("|"))
        return;

    if (m_auth && user && user->level < VC_ONLY_LEVEL)
        return;

    quint32 widgetID = cmdList[0].toUInt();
    VCWidget *widget = m_vc->widget(widgetID);
    int value = 0;
    if (cmdList.count() > 1)
        value = cmdList[1].toInt();

    if (widget == nullptr)
        return;

    switch (widget->type())
    {
        case VCWidget::ButtonWidget:
        {
            VCButton *button = qobject_cast<VCButton*>(widget);
            if (button != nullptr)
                button->requestStateChange(value > 0);
        }
        break;
        case VCWidget::SliderWidget:
        {
            VCSlider *slider = qobject_cast<VCSlider*>(widget);
            if (slider != nullptr)
            {
                if (cmdList.count() > 1 && cmdList[1] == "SLIDER_OVERRIDE")
                {
                    bool enable = cmdList.count() > 2 ? (cmdList[2].toInt() > 0) : false;
                    slider->setIsOverriding(enable);
                }
                else if (cmdList.count() > 2 && cmdList[1] == "CNG_PRESET")
                {
                    slider->setClickAndGoPresetValue(cmdList[2].toInt());
                }
                else if (cmdList.count() > 3 && cmdList[1] == "CNG_COLORS")
                {
                    QColor primary(cmdList[2]);
                    QColor secondary(cmdList[3]);
                    if (primary.isValid() == false)
                        primary = QColor();
                    if (secondary.isValid() == false)
                        secondary = QColor();
                    slider->setClickAndGoColors(primary, secondary);
                }
                else
                {
                    slider->setValue(value, true, true);
                }
            }
        }
        break;
        case VCWidget::AudioTriggersWidget:
        {
            VCAudioTriggers *triggers = qobject_cast<VCAudioTriggers*>(widget);
            if (triggers != nullptr)
            {
                if (cmdList.count() > 2 && cmdList[1] == "AUDIO_VOLUME")
                {
                    int volume = cmdList[2].toInt();
                    if (volume < 0)
                        volume = 0;
                    else if (volume > 100)
                        volume = 100;
                    triggers->setVolumeLevel(uchar(volume));
                }
                else
                {
                    bool ok = false;
                    int enabledValue = cmdList.count() > 1 ? cmdList[1].toInt(&ok) : 0;
                    if (ok)
                        triggers->setCaptureEnabled(enabledValue > 0);
                }
            }
        }
        break;
        case VCWidget::CueListWidget:
        {
            if (cmdList.count() < 2)
                return;

            VCCueList *cue = qobject_cast<VCCueList*>(widget);
            if (cue == nullptr)
                return;

            if (cmdList[1] == "PLAY")
                cue->playClicked();
            else if (cmdList[1] == "STOP")
                cue->stopClicked();
            else if (cmdList[1] == "PREV")
                cue->previousClicked();
            else if (cmdList[1] == "NEXT")
                cue->nextClicked();
            else if (cmdList[1] == "STEP" && cmdList.count() > 2)
                cue->setPlaybackIndex(cmdList[2].toInt());
            else if (cmdList[1] == "CUE_STEP_NOTE" && cmdList.count() > 3)
                cue->setStepNote(cmdList[2].toInt(), cmdList[3]);
            else if (cmdList[1] == "CUE_SIDECHANGE" && cmdList.count() > 2)
                cue->setSideFaderLevel(cmdList[2].toInt());
        }
        break;
        case VCWidget::FrameWidget:
        case VCWidget::SoloFrameWidget:
        {
            if (cmdList.count() < 2)
                return;

            VCFrame *frame = qobject_cast<VCFrame*>(widget);
            if (frame == nullptr)
                return;

            if (cmdList[1] == "NEXT_PG")
            {
                int nextPage = frame->currentPage() + 1;
                if (nextPage >= frame->totalPagesNumber())
                    nextPage = frame->pagesLoop() ? 0 : frame->currentPage();
                setFramePageCompat(frame, nextPage);
            }
            else if (cmdList[1] == "PREV_PG")
            {
                int prevPage = frame->currentPage() - 1;
                if (prevPage < 0)
                    prevPage = frame->pagesLoop() ? frame->totalPagesNumber() - 1 : frame->currentPage();
                setFramePageCompat(frame, prevPage);
            }
            else if (cmdList[1] == "PAGE" && cmdList.count() > 2)
                setFramePageCompat(frame, cmdList[2].toInt());
            else if (cmdList[1] == "FRAME_DISABLE" && cmdList.count() > 2)
                frame->setDisabled(cmdList[2] == "1");
            else if (cmdList[1] == "COLLAPSE" && cmdList.count() > 2)
                frame->setCollapsed(cmdList[2].toInt() == 1);
        }
        break;
        case VCWidget::AnimationWidget:
        {
            if (cmdList.count() < 2)
                return;

            VCAnimation *animation = qobject_cast<VCAnimation*>(widget);
            if (animation == nullptr)
                return;

            if (cmdList[1] == "MATRIX_SLIDER" && cmdList.count() > 2)
                animation->setFaderLevel(cmdList[2].toInt());
            else if (cmdList[1] == "MATRIX_COLOR_1" && cmdList.count() > 2)
                animation->setColor1(QColor(cmdList[2]));
            else if (cmdList[1] == "MATRIX_COLOR_2" && cmdList.count() > 2)
                animation->setColor2(QColor(cmdList[2]));
            else if (cmdList[1] == "MATRIX_COLOR_3" && cmdList.count() > 2)
                animation->setColor3(QColor(cmdList[2]));
            else if (cmdList[1] == "MATRIX_COLOR_4" && cmdList.count() > 2)
                animation->setColor4(QColor(cmdList[2]));
            else if (cmdList[1] == "MATRIX_COLOR_5" && cmdList.count() > 2)
                animation->setColor5(QColor(cmdList[2]));
            else if (cmdList[1] == "MATRIX_COMBO" && cmdList.count() > 2)
                animation->setAlgorithmIndex(cmdList[2].toInt());
        }
        break;
        case VCWidget::XYPadWidget:
        {
            VCXYPad *xypad = qobject_cast<VCXYPad*>(widget);
            if (xypad == nullptr || cmdList.count() < 2)
                return;

            if (cmdList[1] == "XYPAD")
            {
                if (cmdList.count() < 4)
                    return;
                qreal x = cmdList[2].toDouble();
                qreal y = cmdList[3].toDouble();
                xypad->setCurrentPosition(QPointF(x, y));
            }
            else if (cmdList[1] == "XYPAD_RANGE_H")
            {
                if (cmdList.count() < 4)
                    return;
                qreal minVal = cmdList[2].toDouble();
                qreal maxVal = cmdList[3].toDouble();
                xypad->setHorizontalRange(QPointF(minVal, maxVal));
            }
            else if (cmdList[1] == "XYPAD_RANGE_V")
            {
                if (cmdList.count() < 4)
                    return;
                qreal minVal = cmdList[2].toDouble();
                qreal maxVal = cmdList[3].toDouble();
                xypad->setVerticalRange(QPointF(minVal, maxVal));
            }
            else if (cmdList[1] == "XYPAD_PRESET")
            {
                if (cmdList.count() < 3)
                    return;
                xypad->applyPreset(cmdList[2].toUInt());
            }
        }
        break;
        case VCWidget::SpeedWidget:
        {
            if (cmdList.count() < 2)
                return;

            VCSpeedDial *dial = qobject_cast<VCSpeedDial*>(widget);
            if (dial == nullptr)
                return;

            if (cmdList[1] == "SPEED_UP")
                dial->increaseSpeedFactor();
            else if (cmdList[1] == "SPEED_DOWN")
                dial->decreaseSpeedFactor();
            else if (cmdList[1] == "SPEED_APPLY")
                dial->applyFunctionsTime();
            else if (cmdList[1] == "SPEED_TIME" && cmdList.count() > 2)
                dial->setCurrentTime(cmdList[2].toUInt());
            else if (cmdList[1] == "SPEED_FACTOR" && cmdList.count() > 2)
                dial->setCurrentFactor(static_cast<VCSpeedDial::SpeedMultiplier>(cmdList[2].toInt()));
        }
        break;
        case VCWidget::ClockWidget:
        {
            if (cmdList.count() < 2)
                return;

            VCClock *clock = qobject_cast<VCClock*>(widget);
            if (clock == nullptr)
                return;

            if (cmdList[1] == "CLOCK_PLAY")
            {
                // Optional desired state (1/0) for reliable start/stop synchronization.
                bool desiredRunning = !clock->timerRunning();
                if (cmdList.count() > 2)
                    desiredRunning = (cmdList[2] == "1" || cmdList[2].compare("true", Qt::CaseInsensitive) == 0);

                if (desiredRunning != clock->timerRunning())
                    clock->playPauseTimer();
            }
            else if (cmdList[1] == "CLOCK_RESET")
                clock->resetTimer();
            else if (cmdList[1] == "S") // backward compatibility with legacy web client
                clock->playPauseTimer();
            else if (cmdList[1] == "R") // backward compatibility with legacy web client
                clock->resetTimer();
        }
        break;
        default:
        break;
    }
}

void WebAccessQml::slotHandleWebSocketClose(QHttpConnection *conn)
{
    qDebug() << "Websocket Connection closed";
    cleanupDmxSubscription(conn);
    if (conn->userData)
    {
        WebAccessUser* user = static_cast<WebAccessUser*>(conn->userData);
        delete user;
        conn->userData = nullptr;
    }
    conn->deleteLater();

    m_webSocketsList.removeOne(conn);
}

void WebAccessQml::slotFunctionStarted(quint32 fid)
{
    QString wsMessage = QString("FUNCTION|%1|Running").arg(fid);
    sendWebSocketMessage(wsMessage);
}

void WebAccessQml::slotFunctionStopped(quint32 fid)
{
    QString wsMessage = QString("FUNCTION|%1|Stopped").arg(fid);
    sendWebSocketMessage(wsMessage);
}

void WebAccessQml::slotDocLoaded()
{
    m_pendingProjectLoaded = true;
    m_connectedWidgets.clear();
}

void WebAccessQml::slotSelectedPageChanged(int page)
{
    sendWebSocketMessage(QString("VC_PAGE|%1").arg(page));
}

QString WebAccessQml::webFilePath(const QString &relativePath) const
{
    QStringList candidates;
    candidates << QDir::cleanPath(QString("%1/webaccess/res/%2")
                                  .arg(QDir::currentPath())
                                  .arg(relativePath));

    // Development-friendly lookup: walk up from executable directory and
    // locate a sibling "webaccess/res" folder from any parent.
    QDir probeDir(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 6; i++)
    {
        candidates << QDir::cleanPath(QString("%1/webaccess/res/%2")
                                      .arg(probeDir.absolutePath())
                                      .arg(relativePath));
        if (probeDir.cdUp() == false)
            break;
    }

    candidates << QDir::cleanPath(QString("%1%2%3")
                                  .arg(QLCFile::systemDirectory(WEBFILESDIR).path())
                                  .arg(QDir::separator())
                                  .arg(relativePath));

    for (const QString &path : candidates)
    {
        if (QFile::exists(path))
            return path;
    }

    return candidates.last();
}

bool WebAccessQml::serveVCNextFile(QHttpResponse *resp, const QString &relativePath) const
{
    // Security: reject path traversal attempts
    if (relativePath.contains("..") || relativePath.contains('\\'))
        return false;

    // Resolve the vc-next directory from multiple candidate locations
    QStringList distRoots;

    // Dev: source tree relative to cwd
    distRoots << QDir::cleanPath(QString("%1/webaccess/vc-next/dist")
                                  .arg(QDir::currentPath()));

    // Dev: walk up from executable (handles build/ subdirectory)
    QDir probeDir(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 6; i++)
    {
        distRoots << QDir::cleanPath(QString("%1/webaccess/vc-next/dist")
                                      .arg(probeDir.absolutePath()));
        if (probeDir.cdUp() == false)
            break;
    }

    // Installed: system directory (e.g. Contents/Resources/Web/vc-next on macOS)
    distRoots << QDir::cleanPath(
        QLCFile::systemDirectory(WEBFILESDIR).path() + "/vc-next");

    for (const QString &root : distRoots)
    {
        QString candidate = QDir::cleanPath(root + relativePath);
        // Security: verify resolved path is still within the dist directory
        QString canonicalRoot = QDir(root).canonicalPath();
        if (canonicalRoot.isEmpty())
            continue;
        QFileInfo fi(candidate);
        QString canonicalFile = fi.canonicalFilePath();
        if (canonicalFile.isEmpty() || !canonicalFile.startsWith(canonicalRoot + "/"))
            continue;
        return sendFile(resp, canonicalFile, mimeTypeForPath(canonicalFile));
    }
    return false;
}

void WebAccessQml::sendMatrixState(const VCAnimation *animation) const
{
    if (animation == nullptr)
        return;

    QString wsMessage = QString("%1|MATRIX_STATE|%2|%3|%4|%5|%6|%7|%8")
            .arg(animation->id())
            .arg(animation->faderLevel())
            .arg(animation->algorithmIndex())
            .arg(colorToString(animation->getColor1()))
            .arg(colorToString(animation->getColor2()))
            .arg(colorToString(animation->getColor3()))
            .arg(colorToString(animation->getColor4()))
            .arg(colorToString(animation->getColor5()));
    sendWebSocketMessage(wsMessage);
}

QString WebAccessQml::widgetBackgroundImagePath(const VCWidget *widget) const
{
    if (widget == nullptr || widget->backgroundImage().isEmpty())
        return QString();

    QString imgPath = widget->backgroundImage();
#if defined(WIN32) || defined(Q_OS_WIN)
    if (imgPath.contains(':'))
    {
        imgPath.prepend('/');
        imgPath.replace(':', '/');
    }
#endif
    return imgPath;
}

QJsonObject WebAccessQml::baseWidgetToJson(const VCWidget *widget)
{
    QJsonObject obj;
    if (widget == nullptr)
        return obj;

    setupWidgetConnections(widget);

    obj["id"] = int(widget->id());
    obj["type"] = VCWidget::typeToString(widget->type());
    obj["typeId"] = widget->type();
    obj["page"] = widget->page();
    obj["caption"] = widget->caption();
    obj["geometry"] = rectToJson(widget->geometry());
    obj["visible"] = widget->isVisible();
    obj["disabled"] = widget->isDisabled();
    obj["bgColor"] = colorToString(widget->backgroundColor());
    obj["fgColor"] = colorToString(widget->foregroundColor());
    obj["bgImage"] = widgetBackgroundImagePath(widget);
    obj["font"] = fontToJson(widget->font());

    return obj;
}

QJsonObject WebAccessQml::frameToJson(const VCFrame *frame)
{
    QJsonObject obj = baseWidgetToJson(frame);
    obj["showHeader"] = frame->showHeader();
    obj["showEnable"] = frame->showEnable();
    obj["isCollapsed"] = frame->isCollapsed();
    obj["multiPageMode"] = frame->multiPageMode();
    obj["pagesLoop"] = frame->pagesLoop();
    obj["currentPage"] = frame->currentPage();
    obj["totalPages"] = frame->totalPagesNumber();
    QJsonArray labels;
    for (const QString &label : frame->pageLabels())
        labels.append(label);
    obj["pageLabels"] = labels;

    QJsonArray children;
    QList<VCWidget *> childList = frame->children(false);
    for (const VCWidget *child : childList)
        children.append(widgetToJson(child));
    obj["children"] = children;

    return obj;
}

QJsonObject WebAccessQml::widgetToJson(const VCWidget *widget)
{
    QJsonObject obj = baseWidgetToJson(widget);
    if (widget == nullptr)
        return obj;

    switch (widget->type())
    {
        case VCWidget::ButtonWidget:
        {
            const VCButton *button = qobject_cast<const VCButton*>(widget);
            obj["state"] = button->state();
            obj["actionType"] = button->actionType();
            obj["functionId"] = int(button->functionID());
        }
        break;
        case VCWidget::SliderWidget:
        {
            const VCSlider *slider = qobject_cast<const VCSlider*>(widget);
            obj["value"] = slider->value();
            obj["rangeLow"] = slider->rangeLowLimit();
            obj["rangeHigh"] = slider->rangeHighLimit();
            obj["sliderMode"] = VCSlider::sliderModeToString(slider->sliderMode());
            obj["widgetStyle"] = slider->widgetStyleToString(slider->widgetStyle());
            obj["valueDisplay"] = VCSlider::valueDisplayStyleToString(slider->valueDisplayStyle());
            obj["inverted"] = slider->invertedAppearance();
            obj["monitor"] = slider->monitorEnabled();
            obj["isOverriding"] = slider->isOverriding();
            obj["clickAndGoType"] = VCSlider::clickAndGoTypeToString(slider->clickAndGoType());
            obj["cngPrimaryColor"] = colorToString(slider->cngPrimaryColor());
            obj["cngSecondaryColor"] = colorToString(slider->cngSecondaryColor());
            obj["cngPresetResource"] = resourcePathToWebUrl(slider->cngPresetResource());
            obj["cngPresets"] = sliderClickAndGoPresetsToJson(slider, m_doc);
        }
        break;
        case VCWidget::XYPadWidget:
        {
            const VCXYPad *xypad = qobject_cast<const VCXYPad*>(widget);
            QPointF pos = xypad->currentPosition();
            QJsonObject posObj;
            posObj["x"] = pos.x();
            posObj["y"] = pos.y();
            obj["position"] = posObj;
            QJsonObject hRange;
            hRange["min"] = xypad->horizontalRange().x();
            hRange["max"] = xypad->horizontalRange().y();
            obj["horizontalRange"] = hRange;
            QJsonObject vRange;
            vRange["min"] = xypad->verticalRange().x();
            vRange["max"] = xypad->verticalRange().y();
            obj["verticalRange"] = vRange;
            obj["invertedAppearance"] = xypad->invertedAppearance();
            obj["displayMode"] = int(xypad->displayMode());
            obj["presetsList"] = QJsonArray::fromVariantList(xypad->presetsList());
            obj["activePresetId"] = xypad->activePresetId();
        }
        break;
        case VCWidget::FrameWidget:
        case VCWidget::SoloFrameWidget:
            return frameToJson(qobject_cast<const VCFrame*>(widget));
        case VCWidget::CueListWidget:
        {
            const VCCueList *cue = qobject_cast<const VCCueList*>(widget);
            obj["chaserId"] = int(cue->chaserID());
            obj["nextPrevBehavior"] = int(cue->nextPrevBehavior());
            obj["playbackLayout"] = int(cue->playbackLayout());
            obj["sideFaderMode"] = int(cue->sideFaderMode());
            obj["sideFaderLevel"] = cue->sideFaderLevel();
            obj["primaryTop"] = cue->primaryTop();
            obj["nextStepIndex"] = cue->nextStepIndex();
            obj["playbackStatus"] = int(cue->playbackStatus());
            obj["playbackIndex"] = cue->playbackIndex();

            QJsonArray steps;
            QVariant listVar = cue->stepsList();
            ListModel *model = qobject_cast<ListModel*>(listVar.value<QObject*>());
            if (model != nullptr)
            {
                for (int i = 0; i < model->rowCount(); i++)
                {
                    QVariant itemVar = model->itemAt(i);
                    if (itemVar.canConvert<QVariantMap>())
                    {
                        QVariantMap map = itemVar.toMap();
                        quint32 funcId = map.value("funcID").toUInt();
                        Function *func = m_doc->function(funcId);
                        if (func != nullptr)
                        {
                            map.insert("funcName", func->name());
                            map.insert("funcType", func->typeString());
                        }
                        QJsonObject stepObj = QJsonObject::fromVariantMap(map);
                        steps.append(stepObj);
                    }
                }
            }
            obj["steps"] = steps;
        }
        break;
        case VCWidget::AudioTriggersWidget:
        {
            const VCAudioTriggers *triggers = qobject_cast<const VCAudioTriggers*>(widget);
            obj["enabled"] = triggers->captureEnabled();
            obj["volume"] = triggers->volumeLevel();
            obj["bars"] = triggers->barsNumber();
        }
        break;
        case VCWidget::ClockWidget:
        {
            const VCClock *clock = qobject_cast<const VCClock*>(widget);
            obj["clockType"] = int(clock->clockType());
            obj["currentTime"] = clock->currentTime();
            obj["targetTime"] = clock->targetTime();
            obj["running"] = clock->timerRunning();
            obj["scheduledDaysMask"] = clockScheduledDaysMask(clock);
        }
        break;
        case VCWidget::AnimationWidget:
        {
            const VCAnimation *animation = qobject_cast<const VCAnimation*>(widget);
            obj["visibilityMask"] = int(animation->visibilityMask());
            obj["functionId"] = int(animation->functionID());
            obj["faderLevel"] = animation->faderLevel();
            obj["instantChanges"] = animation->instantChanges();
            obj["color1"] = colorToString(animation->getColor1());
            obj["color2"] = colorToString(animation->getColor2());
            obj["color3"] = colorToString(animation->getColor3());
            obj["color4"] = colorToString(animation->getColor4());
            obj["color5"] = colorToString(animation->getColor5());
            obj["algorithmIndex"] = animation->algorithmIndex();
            QJsonArray algos;
            for (const QString &name : animation->algorithms())
                algos.append(name);
            obj["algorithms"] = algos;
        }
        break;
        case VCWidget::SpeedWidget:
        {
            const VCSpeedDial *dial = qobject_cast<const VCSpeedDial*>(widget);
            obj["visibilityMask"] = int(dial->visibilityMask());
            obj["timeMin"] = int(dial->timeMinimumValue());
            obj["timeMax"] = int(dial->timeMaximumValue());
            obj["currentTime"] = int(dial->currentTime());
            obj["resetOnDialChange"] = dial->resetOnDialChange();
            obj["currentFactor"] = int(dial->currentFactor());
            obj["presetsList"] = QJsonArray::fromVariantList(dial->presetsList());
        }
        break;
        default:
            break;
    }

    return obj;
}

void WebAccessQml::collectWidgets(const VCFrame *frame, QList<VCWidget *> &list, bool recursive) const
{
    if (frame == nullptr)
        return;

    QList<VCWidget *> children = frame->children(false);
    for (VCWidget *child : children)
    {
        list.append(child);
        if (recursive)
        {
            VCFrame *childFrame = qobject_cast<VCFrame *>(child);
            if (childFrame != nullptr)
                collectWidgets(childFrame, list, recursive);
        }
    }
}

QByteArray WebAccessQml::getVCJson()
{
    QJsonObject root;
    root["version"] = 1;
    QJsonObject appObj;
    appObj["name"] = QString(APPNAME);
    appObj["version"] = QString(APPVERSION);
    root["app"] = appObj;
    root["pixelDensity"] = m_vc->pixelDensity();
    root["selectedPage"] = m_vc->selectedPage();
    QJsonObject uiStyle = loadUiStyleJson();
    if (uiStyle.isEmpty() == false)
        root["uiStyle"] = uiStyle;

    QJsonArray pages;
    for (int i = 0; i < m_vc->pagesCount(); i++)
    {
        VCFrame *page = m_vc->page(i);
        QJsonObject pageObj = frameToJson(page);
        pageObj["index"] = i;
        pages.append(pageObj);
    }
    root["pages"] = pages;

    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

void WebAccessQml::setupWidgetConnections(const VCWidget *widget)
{
    if (widget == nullptr)
        return;

    if (m_connectedWidgets.contains(widget->id()))
        return;

    m_connectedWidgets.insert(widget->id());

    connect(widget, SIGNAL(isVisibleChanged(bool)),
            this, SLOT(slotWidgetVisibilityChanged(bool)));

    switch (widget->type())
    {
        case VCWidget::ButtonWidget:
        {
            const VCButton *button = qobject_cast<const VCButton*>(widget);
            connect(button, SIGNAL(stateChanged(int)),
                    this, SLOT(slotButtonStateChanged(int)));
            connect(button, SIGNAL(disabledStateChanged(bool)),
                    this, SLOT(slotButtonDisableStateChanged(bool)));
        }
        break;
        case VCWidget::LabelWidget:
        {
            const VCLabel *label = qobject_cast<const VCLabel*>(widget);
            connect(label, SIGNAL(disabledStateChanged(bool)),
                    this, SLOT(slotLabelDisableStateChanged(bool)));
        }
        break;
        case VCWidget::SliderWidget:
        {
            const VCSlider *slider = qobject_cast<const VCSlider*>(widget);
            connect(slider, SIGNAL(valueChanged(int)),
                    this, SLOT(slotSliderValueChanged(int)));
            connect(slider, SIGNAL(disabledStateChanged(bool)),
                    this, SLOT(slotSliderDisableStateChanged(bool)));
            connect(slider, SIGNAL(isOverridingChanged()),
                    this, SLOT(slotSliderOverrideChanged()));
            connect(slider, SIGNAL(cngPrimaryColorChanged(QColor)),
                    this, SLOT(slotSliderClickAndGoColorsChanged()));
            connect(slider, SIGNAL(cngSecondaryColorChanged(QColor)),
                    this, SLOT(slotSliderClickAndGoColorsChanged()));
        }
        break;
        case VCWidget::AudioTriggersWidget:
        {
            const VCAudioTriggers *triggers = qobject_cast<const VCAudioTriggers*>(widget);
            connect(triggers, SIGNAL(captureEnabledChanged()),
                    this, SLOT(slotAudioTriggersToggled()));
            connect(triggers, SIGNAL(volumeLevelChanged()),
                    this, SLOT(slotAudioTriggersVolumeChanged()));
            connect(triggers, SIGNAL(disabledStateChanged(bool)),
                    this, SLOT(slotWidgetDisableStateChanged(bool)));
        }
        break;
        case VCWidget::CueListWidget:
        {
            const VCCueList *cue = qobject_cast<const VCCueList*>(widget);
            connect(cue, SIGNAL(playbackIndexChanged(int)),
                    this, SLOT(slotCueIndexChanged(int)));
            connect(cue, SIGNAL(playbackStatusChanged()),
                    this, SLOT(slotCuePlaybackStateChanged()));
            connect(cue, SIGNAL(nextStepIndexChanged()),
                    this, SLOT(slotCuePlaybackStateChanged()));
            connect(cue, SIGNAL(sideFaderLevelChanged()),
                    this, SLOT(slotCueSideFaderLevelChanged()));
            connect(cue, SIGNAL(disabledStateChanged(bool)),
                    this, SLOT(slotCueDisableStateChanged(bool)));
        }
        break;
        case VCWidget::FrameWidget:
        case VCWidget::SoloFrameWidget:
        {
            const VCFrame *frame = qobject_cast<const VCFrame*>(widget);
            connect(frame, SIGNAL(currentPageChanged(int)),
                    this, SLOT(slotFramePageChanged(int)));
            connect(frame, SIGNAL(disabledStateChanged(bool)),
                    this, SLOT(slotFrameDisableStateChanged(bool)));

            QList<VCWidget *> children = frame->children(false);
            for (VCWidget *child : children)
                setupWidgetConnections(child);
        }
        break;
        case VCWidget::AnimationWidget:
        {
            const VCAnimation *animation = qobject_cast<const VCAnimation*>(widget);
            connect(animation, SIGNAL(faderLevelChanged()),
                    this, SLOT(slotMatrixFaderChanged()));
            connect(animation, SIGNAL(color1Changed()),
                    this, SLOT(slotMatrixColorsChanged()));
            connect(animation, SIGNAL(color2Changed()),
                    this, SLOT(slotMatrixColorsChanged()));
            connect(animation, SIGNAL(color3Changed()),
                    this, SLOT(slotMatrixColorsChanged()));
            connect(animation, SIGNAL(color4Changed()),
                    this, SLOT(slotMatrixColorsChanged()));
            connect(animation, SIGNAL(color5Changed()),
                    this, SLOT(slotMatrixColorsChanged()));
            connect(animation, SIGNAL(algorithmIndexChanged()),
                    this, SLOT(slotMatrixAlgorithmChanged()));
            connect(animation, SIGNAL(disabledStateChanged(bool)),
                    this, SLOT(slotWidgetDisableStateChanged(bool)));
        }
        break;
        case VCWidget::XYPadWidget:
        {
            const VCXYPad *xypad = qobject_cast<const VCXYPad*>(widget);
            connect(xypad, SIGNAL(currentPositionChanged()),
                    this, SLOT(slotXYPadPositionChanged()));
            connect(xypad, SIGNAL(activePresetIdChanged()),
                    this, SLOT(slotXYPadPresetChanged()));
            connect(xypad, SIGNAL(disabledStateChanged(bool)),
                    this, SLOT(slotWidgetDisableStateChanged(bool)));
        }
        break;
        case VCWidget::SpeedWidget:
        {
            const VCSpeedDial *dial = qobject_cast<const VCSpeedDial*>(widget);
            connect(dial, SIGNAL(currentTimeChanged()),
                    this, SLOT(slotSpeedDialTimeChanged()));
            connect(dial, SIGNAL(currentFactorChanged()),
                    this, SLOT(slotSpeedDialFactorChanged()));
            connect(dial, SIGNAL(disabledStateChanged(bool)),
                    this, SLOT(slotWidgetDisableStateChanged(bool)));
        }
        break;
        case VCWidget::ClockWidget:
        {
            const VCClock *clock = qobject_cast<const VCClock*>(widget);
            connect(clock, SIGNAL(currentTimeChanged(int)),
                    this, SLOT(slotClockTimeChanged(int)));
            connect(clock, SIGNAL(timerRunningChanged(bool)),
                    this, SLOT(slotClockTimerRunningChanged(bool)));
            connect(clock, SIGNAL(disabledStateChanged(bool)),
                    this, SLOT(slotWidgetDisableStateChanged(bool)));
        }
        break;
        default:
            break;
    }
}

void WebAccessQml::slotButtonStateChanged(int state)
{
    VCButton *btn = qobject_cast<VCButton *>(sender());
    if (btn == nullptr)
        return;

    QString wsMessage = QString("%1|BUTTON|%2").arg(btn->id()).arg(state == VCButton::Active ? 255 :
                                                                  state == VCButton::Monitoring ? 127 : 0);
    sendWebSocketMessage(wsMessage);
}

void WebAccessQml::slotButtonDisableStateChanged(bool disable)
{
    VCButton *btn = qobject_cast<VCButton *>(sender());
    if (btn == nullptr)
        return;

    QString wsMessage = QString("%1|BUTTON_DISABLE|%2").arg(btn->id()).arg(disable);
    sendWebSocketMessage(wsMessage);
}

void WebAccessQml::slotLabelDisableStateChanged(bool disable)
{
    VCLabel *lbl = qobject_cast<VCLabel *>(sender());
    if (lbl == nullptr)
        return;

    QString wsMessage = QString("%1|LABEL_DISABLE|%2").arg(lbl->id()).arg(disable);
    sendWebSocketMessage(wsMessage);
}

void WebAccessQml::slotWidgetVisibilityChanged(bool isVisible)
{
    VCWidget *widget = qobject_cast<VCWidget *>(sender());
    if (widget == nullptr)
        return;

    QString wsMessage = QString("%1|WIDGET_VISIBLE|%2").arg(widget->id()).arg(isVisible);
    sendWebSocketMessage(wsMessage);
}

void WebAccessQml::slotSliderValueChanged(int value)
{
    VCSlider *slider = qobject_cast<VCSlider *>(sender());
    if (slider == nullptr)
        return;

    QString wsMessage = QString("%1|SLIDER|%2|%3").arg(slider->id()).arg(value).arg(sliderDisplayValue(slider));
    sendWebSocketMessage(wsMessage);
}

void WebAccessQml::slotSliderDisableStateChanged(bool disable)
{
    VCSlider *slider = qobject_cast<VCSlider *>(sender());
    if (slider == nullptr)
        return;

    QString wsMessage = QString("%1|SLIDER_DISABLE|%2").arg(slider->id()).arg(disable);
    sendWebSocketMessage(wsMessage);
}

void WebAccessQml::slotSliderOverrideChanged()
{
    VCSlider *slider = qobject_cast<VCSlider *>(sender());
    if (slider == nullptr)
        return;

    QString wsMessage = QString("%1|SLIDER_OVERRIDE|%2").arg(slider->id()).arg(slider->isOverriding());
    sendWebSocketMessage(wsMessage);
}

void WebAccessQml::slotSliderClickAndGoColorsChanged()
{
    VCSlider *slider = qobject_cast<VCSlider *>(sender());
    if (slider == nullptr)
        return;

    QString wsMessage = QString("%1|CNG_COLORS|%2|%3")
            .arg(slider->id())
            .arg(colorToString(slider->cngPrimaryColor()))
            .arg(colorToString(slider->cngSecondaryColor()));
    sendWebSocketMessage(wsMessage);
}

void WebAccessQml::slotAudioTriggersToggled()
{
    VCAudioTriggers *triggers = qobject_cast<VCAudioTriggers *>(sender());
    if (triggers == nullptr)
        return;

    QString wsMessage = QString("%1|AUDIOTRIGGERS|%2").arg(triggers->id()).arg(triggers->captureEnabled());
    sendWebSocketMessage(wsMessage);
}

void WebAccessQml::slotAudioTriggersVolumeChanged()
{
    VCAudioTriggers *triggers = qobject_cast<VCAudioTriggers *>(sender());
    if (triggers == nullptr)
        return;

    QString wsMessage = QString("%1|AUDIO_VOLUME|%2").arg(triggers->id()).arg(triggers->volumeLevel());
    sendWebSocketMessage(wsMessage);
}

void WebAccessQml::slotWidgetDisableStateChanged(bool disable)
{
    VCWidget *widget = qobject_cast<VCWidget *>(sender());
    if (widget == nullptr)
        return;

    QString wsMessage = QString("%1|WIDGET_DISABLE|%2").arg(widget->id()).arg(disable);
    sendWebSocketMessage(wsMessage);
}

void WebAccessQml::slotCueIndexChanged(int idx)
{
    VCCueList *cue = qobject_cast<VCCueList *>(sender());
    if (cue == nullptr)
        return;

    QString wsMessage = QString("%1|CUE|%2").arg(cue->id()).arg(idx);
    sendWebSocketMessage(wsMessage);
}

void WebAccessQml::slotCuePlaybackStateChanged()
{
    VCCueList *cue = qobject_cast<VCCueList *>(sender());
    if (cue == nullptr)
        return;

    QString wsMessage = QString("%1|CUE_STATE|%2|%3|%4|%5|%6")
            .arg(cue->id())
            .arg(int(cue->playbackStatus()))
            .arg(cue->playbackIndex())
            .arg(cue->nextStepIndex())
            .arg(cue->sideFaderLevel())
            .arg(cue->primaryTop());
    sendWebSocketMessage(wsMessage);
}

void WebAccessQml::slotCueSideFaderLevelChanged()
{
    VCCueList *cue = qobject_cast<VCCueList *>(sender());
    if (cue == nullptr)
        return;

    QString wsMessage = QString("%1|CUE_SIDE|%2|%3").arg(cue->id()).arg(cue->sideFaderLevel()).arg(cue->primaryTop());
    sendWebSocketMessage(wsMessage);
}

void WebAccessQml::slotCueDisableStateChanged(bool disable)
{
    VCCueList *cue = qobject_cast<VCCueList *>(sender());
    if (cue == nullptr)
        return;

    QString wsMessage = QString("%1|CUE_DISABLE|%2").arg(cue->id()).arg(disable);
    sendWebSocketMessage(wsMessage);
}

void WebAccessQml::slotFramePageChanged(int pageNum)
{
    VCFrame *frame = qobject_cast<VCFrame *>(sender());
    if (frame == nullptr)
        return;

    QString wsMessage = QString("%1|FRAME|%2").arg(frame->id()).arg(pageNum);
    sendWebSocketMessage(wsMessage);
}

void WebAccessQml::slotFrameDisableStateChanged(bool disable)
{
    VCFrame *frame = qobject_cast<VCFrame *>(sender());
    if (frame == nullptr)
        return;

    QString wsMessage = QString("%1|FRAME_DISABLE|%2").arg(frame->id()).arg(disable);
    sendWebSocketMessage(wsMessage);
}

void WebAccessQml::slotMatrixFaderChanged()
{
    VCAnimation *animation = qobject_cast<VCAnimation *>(sender());
    if (animation == nullptr)
        return;

    sendMatrixState(animation);
}

void WebAccessQml::slotMatrixColorsChanged()
{
    VCAnimation *animation = qobject_cast<VCAnimation *>(sender());
    if (animation == nullptr)
        return;

    sendMatrixState(animation);
}

void WebAccessQml::slotMatrixAlgorithmChanged()
{
    VCAnimation *animation = qobject_cast<VCAnimation *>(sender());
    if (animation == nullptr)
        return;

    sendMatrixState(animation);
}

void WebAccessQml::slotXYPadPositionChanged()
{
    VCXYPad *xypad = qobject_cast<VCXYPad *>(sender());
    if (xypad == nullptr)
        return;

    QPointF pos = xypad->currentPosition();
    QString wsMessage = QString("%1|XYPAD|%2|%3").arg(xypad->id()).arg(pos.x()).arg(pos.y());
    sendWebSocketMessage(wsMessage);
}

void WebAccessQml::slotXYPadPresetChanged()
{
    VCXYPad *xypad = qobject_cast<VCXYPad *>(sender());
    if (xypad == nullptr)
        return;

    QString wsMessage = QString("%1|XYPAD_PRESET|%2").arg(xypad->id()).arg(xypad->activePresetId());
    sendWebSocketMessage(wsMessage);
}

void WebAccessQml::slotSpeedDialTimeChanged()
{
    VCSpeedDial *dial = qobject_cast<VCSpeedDial *>(sender());
    if (dial == nullptr)
        return;

    QString wsMessage = QString("%1|SPEED_STATE|%2|%3")
            .arg(dial->id())
            .arg(dial->currentTime())
            .arg(int(dial->currentFactor()));
    sendWebSocketMessage(wsMessage);
}

void WebAccessQml::slotSpeedDialFactorChanged()
{
    VCSpeedDial *dial = qobject_cast<VCSpeedDial *>(sender());
    if (dial == nullptr)
        return;

    QString wsMessage = QString("%1|SPEED_STATE|%2|%3")
            .arg(dial->id())
            .arg(dial->currentTime())
            .arg(int(dial->currentFactor()));
    sendWebSocketMessage(wsMessage);
}

void WebAccessQml::slotClockTimeChanged(int time)
{
    VCClock *clock = qobject_cast<VCClock *>(sender());
    if (clock == nullptr)
        return;
    if (isWidgetVisibleForWeb(clock, m_vc) == false)
        return;

    if (clock->clockType() == VCClock::Clock)
    {
        QString wsMessage = QString("%1|CLOCK|%2").arg(clock->id()).arg(time);
        sendWebSocketMessage(wsMessage);
        return;
    }

    // For stopwatch/countdown send only second references.
    if (time != 0 && (time % 1000) != 0)
        return;

    QString wsMessage = QString("%1|CLOCK|%2|%3")
            .arg(clock->id())
            .arg(time)
            .arg(clock->timerRunning() ? 1 : 0);
    sendWebSocketMessage(wsMessage);
}

void WebAccessQml::slotClockTimerRunningChanged(bool running)
{
    Q_UNUSED(running)
    VCClock *clock = qobject_cast<VCClock *>(sender());
    if (clock == nullptr)
        return;
    if (isWidgetVisibleForWeb(clock, m_vc) == false)
        return;

    if (clock->clockType() == VCClock::Clock)
        return;

    QString wsMessage = QString("%1|CLOCK|%2|%3")
            .arg(clock->id())
            .arg(clock->currentTime())
            .arg(clock->timerRunning() ? 1 : 0);
    sendWebSocketMessage(wsMessage);
}

void WebAccessQml::slotGrandMasterValueChanged(uchar value)
{
    int p = qFloor(((double(value) / double(UCHAR_MAX)) * double(100)) + 0.5);
    QString gmDisplayValue = QString("%1%").arg(p, 2, 10, QChar('0'));
    QString wsMessage = QString("GM_VALUE|%1|%2").arg(value).arg(gmDisplayValue);
    sendWebSocketMessage(wsMessage);
}

void WebAccessQml::handleAutostartProject(const QString &path)
{
    emit storeAutostartProject(path);
}

// ---------- REST API: /api/fixtures and /api/channels ----------

namespace {

QJsonObject physicalToJsonObj(const QLCPhysical &phy)
{
    QJsonObject o;
    if (phy.weight() > 0) o.insert("weight", phy.weight());
    if (phy.width() > 0) o.insert("width", phy.width());
    if (phy.height() > 0) o.insert("height", phy.height());
    if (phy.depth() > 0) o.insert("depth", phy.depth());
    if (!phy.bulbType().isEmpty()) o.insert("bulbType", phy.bulbType());
    if (phy.bulbLumens() > 0) o.insert("bulbLumens", phy.bulbLumens());
    if (phy.bulbColourTemperature() > 0) o.insert("bulbColourTemperature", phy.bulbColourTemperature());
    if (!phy.lensName().isEmpty()) o.insert("lensName", phy.lensName());
    if (phy.lensDegreesMin() > 0) o.insert("lensDegreesMin", phy.lensDegreesMin());
    if (phy.lensDegreesMax() > 0) o.insert("lensDegreesMax", phy.lensDegreesMax());
    if (!phy.focusType().isEmpty()) o.insert("focusType", phy.focusType());
    if (phy.focusPanMax() > 0) o.insert("focusPanMax", phy.focusPanMax());
    if (phy.focusTiltMax() > 0) o.insert("focusTiltMax", phy.focusTiltMax());
    if (phy.powerConsumption() > 0) o.insert("powerConsumption", (int)phy.powerConsumption());
    if (!phy.dmxConnector().isEmpty()) o.insert("dmxConnector", phy.dmxConnector());
    return o;
}

QJsonArray fixtureCapabilitiesArr(const Fixture *fxi)
{
    QSet<QString> seen;
    QJsonArray caps;
    bool hasPan = false, hasTilt = false;
    bool hasR = false, hasG = false, hasB = false;
    bool hasC = false, hasM = false, hasY = false;
    bool hasW = false, hasA = false, hasUV = false;
    bool hasContinuousPan = false, hasContinuousTilt = false;

    auto addOnce = [&](const QString &c) {
        if (!seen.contains(c)) { seen.insert(c); caps.append(c); }
    };

    for (quint32 ch = 0; ch < fxi->channels(); ch++)
    {
        const QLCChannel *channel = fxi->channel(ch);
        if (!channel) continue;
        switch (channel->group())
        {
            case QLCChannel::Pan:
                hasPan = true;
                if (!hasContinuousPan)
                {
                    for (const QLCCapability *cap : channel->capabilities())
                    {
                        auto p = cap->preset();
                        if (p >= QLCCapability::RotationClockwise && p <= QLCCapability::RotationCounterClockwiseFastToSlow)
                            { hasContinuousPan = true; break; }
                    }
                }
                break;
            case QLCChannel::Tilt:
                hasTilt = true;
                if (!hasContinuousTilt)
                {
                    for (const QLCCapability *cap : channel->capabilities())
                    {
                        auto p = cap->preset();
                        if (p >= QLCCapability::RotationClockwise && p <= QLCCapability::RotationCounterClockwiseFastToSlow)
                            { hasContinuousTilt = true; break; }
                    }
                }
                break;
            case QLCChannel::Colour: addOnce("Colour"); break;
            case QLCChannel::Gobo: addOnce("Gobo"); break;
            case QLCChannel::Shutter: addOnce("Shutter"); break;
            case QLCChannel::Beam: addOnce("Beam"); break;
            case QLCChannel::Prism: addOnce("Prism"); break;
            case QLCChannel::Intensity:
                switch (channel->colour())
                {
                    case QLCChannel::Red: hasR = true; break;
                    case QLCChannel::Green: hasG = true; break;
                    case QLCChannel::Blue: hasB = true; break;
                    case QLCChannel::Cyan: hasC = true; break;
                    case QLCChannel::Magenta: hasM = true; break;
                    case QLCChannel::Yellow: hasY = true; break;
                    case QLCChannel::White: hasW = true; break;
                    case QLCChannel::Amber: hasA = true; break;
                    case QLCChannel::UV: hasUV = true; break;
                    default: break;
                }
                break;
            default: break;
        }
    }
    if (hasPan && hasTilt) caps.append("Pan/Tilt");
    if (hasR && hasG && hasB)
    {
        if (hasW) caps.append("RGBW");
        else caps.append("RGB");
    }
    if (hasC && hasM && hasY) caps.append("CMY");
    if (hasA) caps.append("Amber");
    if (hasUV) caps.append("UV");
    if (hasContinuousPan) caps.append("ContinuousPanRotation");
    if (hasContinuousTilt) caps.append("ContinuousTiltRotation");
    return caps;
}

QJsonObject fixtureToJsonObj(const Fixture *fxi)
{
    QJsonObject entry;
    entry.insert("id", (qint64)fxi->id());
    entry.insert("name", fxi->name());
    entry.insert("universe", (int)fxi->universe());
    entry.insert("address", (int)fxi->address());
    entry.insert("channels", (int)fxi->channels());
    entry.insert("heads", fxi->heads());

    if (fxi->fixtureDef())
    {
        entry.insert("manufacturer", fxi->fixtureDef()->manufacturer());
        entry.insert("model", fxi->fixtureDef()->model());
        entry.insert("type", QLCFixtureDef::typeToString(fxi->fixtureDef()->type()));
    }

    if (fxi->fixtureMode())
    {
        entry.insert("mode", fxi->fixtureMode()->name());

        const auto &modeHeads = fxi->fixtureMode()->heads();
        if (!modeHeads.isEmpty())
        {
            QJsonArray headMap;
            for (int h = 0; h < modeHeads.size(); h++)
            {
                const QLCFixtureHead &head = modeHeads[h];
                QJsonObject hEntry;
                hEntry.insert("index", h);
                QJsonArray chList;
                for (quint32 c : head.channels())
                    chList.append((int)c);
                hEntry.insert("channels", chList);

                QVector<quint32> rgb = fxi->rgbChannels(h);
                if (rgb.size() == 3)
                {
                    QJsonArray a;
                    a.append((int)rgb[0]); a.append((int)rgb[1]); a.append((int)rgb[2]);
                    hEntry.insert("rgbChannels", a);
                }
                QVector<quint32> cmy = fxi->cmyChannels(h);
                if (cmy.size() == 3)
                {
                    QJsonArray a;
                    a.append((int)cmy[0]); a.append((int)cmy[1]); a.append((int)cmy[2]);
                    hEntry.insert("cmyChannels", a);
                }
                headMap.append(hEntry);
            }
            entry.insert("headMap", headMap);
        }

        QJsonObject phyJson = physicalToJsonObj(fxi->fixtureMode()->physical());
        if (!phyJson.isEmpty())
            entry.insert("physical", phyJson);
    }

    entry.insert("capabilities", fixtureCapabilitiesArr(fxi));
    return entry;
}

QJsonObject capabilityToJsonObj(const QLCCapability *cap)
{
    QJsonObject o;
    o.insert("min", (int)cap->min());
    o.insert("max", (int)cap->max());
    o.insert("name", cap->name());
    QString presetStr = QLCCapability::presetToString(cap->preset());
    if (!presetStr.isEmpty() && cap->preset() != QLCCapability::Custom)
        o.insert("preset", presetStr);

    switch (cap->presetType())
    {
        case QLCCapability::SingleColor:
        {
            QVariant res = cap->resource(0);
            if (res.isValid())
                o.insert("color1", res.value<QColor>().name());
            break;
        }
        case QLCCapability::DoubleColor:
        {
            QVariant res0 = cap->resource(0);
            QVariant res1 = cap->resource(1);
            if (res0.isValid()) o.insert("color1", res0.value<QColor>().name());
            if (res1.isValid()) o.insert("color2", res1.value<QColor>().name());
            break;
        }
        case QLCCapability::Picture:
        {
            QVariant res = cap->resource(0);
            if (res.isValid())
                o.insert("image", res.toString());
            break;
        }
        case QLCCapability::SingleValue:
        {
            QVariant res = cap->resource(0);
            if (res.isValid())
                o.insert("value", res.toFloat());
            QString units = cap->presetUnits();
            if (!units.isEmpty()) o.insert("unit", units);
            break;
        }
        case QLCCapability::DoubleValue:
        {
            QVariant res0 = cap->resource(0);
            QVariant res1 = cap->resource(1);
            if (res0.isValid()) o.insert("valueMin", res0.toFloat());
            if (res1.isValid()) o.insert("valueMax", res1.toFloat());
            QString units = cap->presetUnits();
            if (!units.isEmpty()) o.insert("unit", units);
            break;
        }
        default:
            break;
    }
    return o;
}

} // namespace

QByteArray WebAccessQml::buildFixturesJson()
{
    QJsonArray arr;
    if (m_doc)
    {
        for (Fixture *fxi : m_doc->fixtures())
        {
            if (!fxi) continue;
            arr.append(fixtureToJsonObj(fxi));
        }
    }
    return QJsonDocument(arr).toJson(QJsonDocument::Compact);
}

QByteArray WebAccessQml::buildChannelsJson(const QList<quint32> &fixtureIDs)
{
    QJsonArray arr;
    if (!m_doc) return QJsonDocument(arr).toJson(QJsonDocument::Compact);

    QList<Fixture *> targets;
    if (fixtureIDs.isEmpty())
    {
        targets = m_doc->fixtures();
    }
    else
    {
        for (quint32 id : fixtureIDs)
        {
            Fixture *f = m_doc->fixture(id);
            if (f) targets.append(f);
        }
    }

    for (Fixture *fxi : targets)
    {
        if (!fxi) continue;
        for (quint32 ch = 0; ch < fxi->channels(); ch++)
        {
            const QLCChannel *channel = fxi->channel(ch);
            if (!channel) continue;

            QJsonObject entry;
            entry.insert("fixtureID", (qint64)fxi->id());
            entry.insert("index", (int)ch);
            entry.insert("name", channel->name());
            entry.insert("group", QLCChannel::groupToString(channel->group()));
            entry.insert("colour", QLCChannel::colourToString(channel->colour()));
            entry.insert("preset", QLCChannel::presetToString(channel->preset()));
            entry.insert("controlByte", channel->controlByte() == QLCChannel::MSB ? "coarse" : "fine");
            entry.insert("defaultValue", (int)channel->defaultValue());

            if (fxi->fixtureMode())
            {
                int headIdx = fxi->fixtureMode()->headForChannel(ch);
                if (headIdx >= 0)
                    entry.insert("headIndex", headIdx);
            }

            QJsonArray caps;
            for (const QLCCapability *cap : channel->capabilities())
                caps.append(capabilityToJsonObj(cap));
            entry.insert("capabilities", caps);

            arr.append(entry);
        }
    }
    return QJsonDocument(arr).toJson(QJsonDocument::Compact);
}

// ─── DMX subscription / push implementation ────────────────────────────

void WebAccessQml::handleDmxJson(QHttpConnection *conn, const QJsonObject &msg)
{
    QString cmd = msg["cmd"].toString();

    if (cmd == "DMX_SUB")
    {
        QJsonArray ids = msg["fixtureIDs"].toArray();
        DmxSubscription &sub = m_dmxSubs[conn];
        sub.lastActivity = QDateTime::currentMSecsSinceEpoch();
        for (const QJsonValue &v : ids)
            sub.fixtureIDs.insert(v.toInt());
        rebuildSubscribedAddrs(conn);
        for (const QJsonValue &v : ids)
            sendDmxSnapshot(conn, v.toInt());
        if (!sub.flushTimer)
        {
            sub.flushTimer = new QTimer(this);
            sub.flushTimer->setInterval(50);
            sub.flushTimer->setSingleShot(false);
            connect(sub.flushTimer, &QTimer::timeout, this, [this, conn]() {
                slotFlushDmxDeltas(conn);
            });
            sub.flushTimer->start();
        }
    }
    else if (cmd == "DMX_UNSUB")
    {
        QJsonArray ids = msg["fixtureIDs"].toArray();
        if (m_dmxSubs.contains(conn))
        {
            DmxSubscription &sub = m_dmxSubs[conn];
            for (const QJsonValue &v : ids)
                sub.fixtureIDs.remove(v.toInt());
            rebuildSubscribedAddrs(conn);
            if (sub.fixtureIDs.isEmpty())
                cleanupDmxSubscription(conn);
        }
    }
    else if (cmd == "DMX_UNSUB_ALL")
    {
        cleanupDmxSubscription(conn);
    }
    else if (cmd == "DMX_HEARTBEAT")
    {
        if (m_dmxSubs.contains(conn))
            m_dmxSubs[conn].lastActivity = QDateTime::currentMSecsSinceEpoch();
    }
}

void WebAccessQml::rebuildSubscribedAddrs(QHttpConnection *conn)
{
    DmxSubscription &sub = m_dmxSubs[conn];
    sub.subscribedAddrs.clear();
    for (quint32 fxID : std::as_const(sub.fixtureIDs))
    {
        Fixture *fxi = m_doc->fixture(fxID);
        if (!fxi) continue;
        quint32 uni = fxi->universe();
        int addr = fxi->address();
        int count = fxi->channels();
        if (!sub.subscribedAddrs.contains(uni))
            sub.subscribedAddrs[uni] = QBitArray(512, false);
        QBitArray &mask = sub.subscribedAddrs[uni];
        for (int i = 0; i < count && (addr + i) < 512; ++i)
            mask.setBit(addr + i, true);
    }
}

void WebAccessQml::sendDmxSnapshot(QHttpConnection *conn, quint32 fixtureID)
{
    Fixture *fxi = m_doc->fixture(fixtureID);
    if (!fxi) return;

    InputOutputMap *io = m_doc->inputOutputMap();
    QList<Universe *> unis = io->claimUniverses();
    quint32 uniIdx = fxi->universe();
    if ((int)uniIdx >= unis.size()) { io->releaseUniverses(false); return; }

    const QByteArray *postGMPtr = unis[uniIdx]->postGMValues();
    const QByteArray postGM = postGMPtr ? QByteArray(*postGMPtr) : QByteArray();
    io->releaseUniverses(false);

    int addr = fxi->address();
    int count = fxi->channels();

    qDebug() << "[DMX-WS] sendDmxSnapshot fxID=" << fixtureID
             << "uni=" << uniIdx << "addr=" << addr << "count=" << count
             << "usingPostGM=true";

    QJsonArray values;
    for (int i = 0; i < count && (addr + i) < postGM.size(); ++i)
        values.append((int)(uchar)postGM.at(addr + i));

    QJsonObject state;
    state["cmd"] = QString("DMX_STATE");
    state["universe"] = (int)uniIdx;
    state["fixtureID"] = (int)fixtureID;
    state["address"] = addr;
    state["count"] = count;
    state["values"] = values;

    conn->webSocketWrite(QString::fromUtf8(QJsonDocument(state).toJson(QJsonDocument::Compact)));

    DmxSubscription &sub = m_dmxSubs[conn];
    if (!sub.lastSent.contains(uniIdx))
        sub.lastSent[uniIdx] = QByteArray(512, 0);
    QByteArray &last = sub.lastSent[uniIdx];
    for (int i = 0; i < count && (addr + i) < last.size() && (addr + i) < postGM.size(); ++i)
        last[addr + i] = postGM.at(addr + i);
}

void WebAccessQml::cleanupDmxSubscription(QHttpConnection *conn)
{
    if (!m_dmxSubs.contains(conn)) return;
    DmxSubscription &sub = m_dmxSubs[conn];
    if (sub.flushTimer)
    {
        sub.flushTimer->stop();
        sub.flushTimer->deleteLater();
    }
    m_dmxSubs.remove(conn);
}

void WebAccessQml::slotUniverseWritten(quint32 uniIdx, QByteArray data)
{
    if (m_dmxSubs.isEmpty())
        return;

    for (auto it = m_dmxSubs.begin(); it != m_dmxSubs.end(); ++it)
    {
        DmxSubscription &sub = it.value();
        if (!sub.subscribedAddrs.contains(uniIdx)) continue;
        const QBitArray &mask = sub.subscribedAddrs[uniIdx];
        QByteArray &last = sub.lastSent[uniIdx];
        if (last.size() != data.size())
            last = QByteArray(data.size(), 0);

        auto &deltas = sub.pendingDeltas[uniIdx];
        for (int a = 0; a < qMin(data.size(), 512); ++a)
        {
            if (!mask.testBit(a)) continue;
            if (data.at(a) != last.at(a))
            {
                last[a] = data.at(a);
                deltas.append(qMakePair(a, (uchar)data.at(a)));
            }
        }
    }
}

void WebAccessQml::slotFlushDmxDeltas(QHttpConnection *conn)
{
    if (!m_dmxSubs.contains(conn)) return;
    DmxSubscription &sub = m_dmxSubs[conn];

    // Heartbeat TTL: auto-release subscription after 30s idle.
    // Do NOT reset Simple Desk channels — the user's fader values should persist
    // even if the web client disconnects. Only clean up the subscription.
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - sub.lastActivity > 30000)
    {
        qDebug() << "[DMX-WS] heartbeat timeout — removing subscription (no fader reset)";
        cleanupDmxSubscription(conn);
        return;
    }

    for (auto uniIt = sub.pendingDeltas.begin(); uniIt != sub.pendingDeltas.end(); ++uniIt)
    {
        auto &deltas = uniIt.value();
        if (deltas.isEmpty()) continue;

        QJsonArray changes;
        for (const auto &pair : std::as_const(deltas))
            changes.append(QJsonArray{pair.first, (int)pair.second});

        QJsonObject msg;
        msg["cmd"] = QString("DMX_DELTA");
        msg["universe"] = (int)uniIt.key();
        msg["changes"] = changes;

        conn->webSocketWrite(QString::fromUtf8(QJsonDocument(msg).toJson(QJsonDocument::Compact)));
        deltas.clear();
    }
}