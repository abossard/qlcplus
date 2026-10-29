/*
  Q Light Controller Plus - Unit test
  pageplanner_test.cpp

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

#include "pageplanner_test.h"

#include <QtTest>
#include <QTranslator>
#include <functional>
#include <QFontMetricsF>
#include <QImage>
#include <QQmlContext>
#include <QQmlExpression>
#include <QQuickItem>
#include <QSettings>

#define private public
#include "app.h"
#include "tardis.h"
#undef private
#include "consolepagecreator.h"
#include "vcpage.h"
#include "resource_paths.h"
#include "contextmanager.h"
#include "fixturemanager.h"
#include "functionmanager.h"
#include "monitorproperties.h"
#include "qlcfixturedefcache.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "qlccapability.h"
#include "fixturecontrols.h"
#include "pageplanner.h"
#include "pagebuilder.h"
#include "stagewizard.h"
#include "fixturegroup.h"
#include "mastertimer.h"
#include "efxfixture.h"
#include "collection.h"
#include "rgbmatrix.h"
#include "chaser.h"
#include "fixture.h"
#include "scene.h"
#include "qlcpalette.h"
#include "efx.h"
#include "doc.h"
#include "inputoutputmap.h"
#include "universe.h"
#include "uimanager.h"
#include "virtualconsole.h"
#include "vccuelist.h"
#include "vcbutton.h"
#include "vcslider.h"
#include "vcxypad.h"
#include "vcanimation.h"
#include "vcframe.h"
#include "vcpage.h"

using ChannelSet = QSet<QPair<quint32, quint32>>;

static QDir shotsDir()
{
    const QString root = qEnvironmentVariable("QLC_PAGE_PLANNER_SHOTS");
    QDir dir(root.isEmpty() ? QDir::temp().filePath("pageplanner-shots") : root);
    dir.mkpath(".");
    return dir;
}

/* The channels the functions $id runs set to fixed values */
static void functionChannels(Doc *doc, quint32 id, ChannelSet &out, QSet<quint32> &seen)
{
    Function *function = doc->function(id);
    if (function == nullptr || seen.contains(id))
        return;
    seen.insert(id);
    if (Scene *scene = qobject_cast<Scene *>(function))
    {
        for (const SceneValue &value : scene->values())
            out.insert({ value.fxi, value.channel });
        // palettes resolve per fixture when the scene runs
        for (quint32 paletteID : scene->palettes())
        {
            QLCPalette *palette = doc->palette(paletteID);
            if (palette == nullptr)
                continue;
            QList<SceneValue> values = palette->valuesFromFixtureGroups(doc, scene->fixtureGroups());
            values.append(palette->valuesFromFixtures(doc, scene->fixtures()));
            for (const SceneValue &value : values)
                out.insert({ value.fxi, value.channel });
        }
    }
    else if (Chaser *chaser = qobject_cast<Chaser *>(function))
    {
        for (const ChaserStep &step : chaser->steps())
            functionChannels(doc, step.fid, out, seen);
    }
    else if (Collection *collection = qobject_cast<Collection *>(function))
    {
        for (quint32 child : collection->functions())
            functionChannels(doc, child, out, seen);
    }
    else if (EFX *efx = qobject_cast<EFX *>(function))
    {
        for (const EFXFixture *ef : efx->fixtures())
        {
            const Fixture *fixture = doc->fixture(ef->head().fxi);
            if (fixture == nullptr)
                continue;
            const QLCFixtureHead head = fixture->head(ef->head().head);
            for (int type : { int(QLCChannel::Pan), int(QLCChannel::Tilt) })
                for (int byte : { int(QLCChannel::MSB), int(QLCChannel::LSB) })
                    if (head.channelNumber(type, byte) != QLCChannel::invalid())
                        out.insert({ fixture->id(), head.channelNumber(type, byte) });
        }
    }
    else if (RGBMatrix *matrix = qobject_cast<RGBMatrix *>(function))
    {
        const FixtureGroup *group = doc->fixtureGroup(matrix->fixtureGroup());
        if (group == nullptr)
            return;
        for (const GroupHead &gh : group->headList())
        {
            const Fixture *fixture = doc->fixture(gh.fxi);
            if (fixture == nullptr)
                continue;
            for (quint32 ch : fixture->rgbChannels(gh.head))
                out.insert({ gh.fxi, ch });
        }
    }
}

namespace
{

struct PageStats
{
    int controls = 0;
    int frames = 0;
    int known = 0;
    int captionMisfits = 0;
    QStringList misfitCaptions;
    int structureErrors = 0;
    int pads = 0;
    ChannelSet continuous;
    ChannelSet fixed;
    QStringList notes;
};

bool knownChannel(const QLCChannel *channel)
{
    if (channel == nullptr)
        return false;
    switch (channel->group())
    {
        case QLCChannel::Intensity:
        case QLCChannel::Pan:
        case QLCChannel::Tilt:
        case QLCChannel::Speed:
            return true;
        default:
            break;
    }
    if (channel->preset() != QLCChannel::Custom)
        return true;
    for (const QLCCapability *cap : channel->capabilities())
        if (cap->preset() != QLCCapability::Custom)
            return true;
    return false;
}

bool captionFits(VCWidget *widget)
{
    const QString caption = widget->caption();
    if (caption.isEmpty())
        return true;
    const QFontMetricsF metrics(widget->font());
    qreal room = widget->logicalGeometry().width() - 4;
    if (qobject_cast<VCFrame *>(widget))
        room -= metrics.height() * 3; // header buttons
    if (qobject_cast<VCSlider *>(widget))
    {
        // slider captions wrap after spaces and slashes, like the QML Text does;
        // every line more takes height from the fader, three at most
        QStringList pieces;
        QString piece;
        for (const QChar c : caption)
        {
            piece += c;
            if (c == ' ' || c == '/')
            {
                pieces << piece;
                piece.clear();
            }
        }
        if (!piece.isEmpty())
            pieces << piece;
        int lines = 1;
        QString line;
        for (const QString &p : pieces)
        {
            if (!line.isEmpty() && metrics.horizontalAdvance((line + p).trimmed()) > room)
            {
                lines++;
                line.clear();
            }
            line += p;
            if (metrics.horizontalAdvance(line.trimmed()) > room)
                return false;
        }
        return lines <= 3;
    }
    if (qobject_cast<VCButton *>(widget))
    {
        for (const QString &word : caption.split(' ', Qt::SkipEmptyParts))
            if (metrics.horizontalAdvance(word) > room)
                return false;
        return true;
    }
    return metrics.horizontalAdvance(caption) <= room;
}

void collect(Doc *doc, VCWidget *widget, PageStats &stats)
{
    if (!captionFits(widget))
    {
        stats.captionMisfits++;
        stats.notes << QString("caption '%1' in %2 px").arg(widget->caption()).arg(widget->logicalGeometry().width());
        stats.misfitCaptions << widget->caption();
    }

    if (VCFrame *frame = qobject_cast<VCFrame *>(widget))
    {
        stats.frames++;
        const QRectF content = frame->contentRect();
        const QList<VCWidget *> children = frame->children(false);
        for (int i = 0; i < children.size(); i++)
        {
            const QRectF rect = children[i]->logicalGeometry();
            if (!content.adjusted(-0.01, -0.01, 0.01, 0.01).contains(rect))
                stats.structureErrors++;
            for (int j = i + 1; j < children.size(); j++)
            {
                const QRectF overlap = rect.intersected(children[j]->logicalGeometry());
                if (children[i]->page() == children[j]->page() && overlap.width() > 0.01 && overlap.height() > 0.01)
                    stats.structureErrors++;
            }
            collect(doc, children[i], stats);
        }
        return;
    }

    stats.controls++;
    QSet<quint32> seen;
    if (VCSlider *slider = qobject_cast<VCSlider *>(widget))
    {
        bool known = slider->clickAndGoType() == VCSlider::CnGColors;
        for (const SceneValue &value : slider->levelChannels())
        {
            const Fixture *fixture = doc->fixture(value.fxi);
            const QLCChannel *channel = fixture ? fixture->channel(value.channel) : nullptr;
            // Click & Go colours set Lime and Indigo to 0: not part of the picked colour
            const bool cngOff = slider->clickAndGoType() == VCSlider::CnGColors && channel &&
                                (channel->colour() == QLCChannel::Lime || channel->colour() == QLCChannel::Indigo);
            if (!cngOff)
                stats.continuous.insert({ value.fxi, value.channel });
            known = known || (fixture && knownChannel(fixture->channel(value.channel)));
        }
        if (slider->sliderMode() != VCSlider::Level)
            known = true;
        stats.known += known ? 1 : 0;
    }
    else if (VCXYPad *pad = qobject_cast<VCXYPad *>(widget))
    {
        stats.pads++;
        for (const VCXYPad::XYPadFixture &entry : pad->fixtures())
        {
            const Fixture *fixture = doc->fixture(entry.m_head.fxi);
            if (fixture == nullptr)
                continue;
            const QLCFixtureHead head = fixture->head(entry.m_head.head);
            for (int type : { int(QLCChannel::Pan), int(QLCChannel::Tilt) })
                for (int byte : { int(QLCChannel::MSB), int(QLCChannel::LSB) })
                    if (head.channelNumber(type, byte) != QLCChannel::invalid())
                        stats.continuous.insert({ fixture->id(), head.channelNumber(type, byte) });
        }
        stats.known++;
    }
    else if (VCButton *button = qobject_cast<VCButton *>(widget))
    {
        functionChannels(doc, button->functionID(), stats.fixed, seen);
        if (button->functionID() != Function::invalidId() || button->actionType() != VCButton::Toggle)
            stats.known++;
    }
    else if (VCCueList *cues = qobject_cast<VCCueList *>(widget))
    {
        functionChannels(doc, cues->chaserID(), stats.fixed, seen);
        stats.known++;
    }
}

/** One synchronous fader pass: Universe::processFaders is protected and the
 *  universe threads do not run in these tests. */
struct UniverseFaders : Universe
{
    static void process(Universe *universe)
    {
        (universe->*(&UniverseFaders::processFaders))(MasterTimer::tick());
    }
};

template <class T> void collectWidgets(VCWidget *widget, QList<T *> &found)
{
    if (T *typed = qobject_cast<T *>(widget))
        found.append(typed);
    if (VCFrame *frame = qobject_cast<VCFrame *>(widget))
        for (VCWidget *child : frame->children(false))
            collectWidgets(child, found);
}

} // namespace

void PagePlanner_Test::initTestCase()
{
    QVERIFY(m_settings.isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settings.path());
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, m_settings.path());
    QCoreApplication::setOrganizationName("QLCPlusPagePlannerTests");
    QCoreApplication::setApplicationName("PagePlanner");
    m_app = std::make_unique<App>();
    m_app->set3dSupported(false);
    m_app->startup();
    m_app->m_uiManager->setModified("scalingFactor", 1.0);
    QVERIFY2(m_app->status() == QQuickView::Ready, qPrintable(m_app->errors().value(0).toString()));
    QQmlExpression startupNotice(qmlContext(m_app->rootObject()), m_app->rootObject(), "scalingInfoPopup");
    QObject *notice = startupNotice.evaluate().value<QObject *>();
    if (notice && notice->property("visible").toBool())
    {
        QVERIFY(QMetaObject::invokeMethod(notice, "accept"));
        QTRY_VERIFY(!notice->property("visible").toBool());
    }
    m_app->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_app.get()));
    m_app->resize(2000, 1250);
    QTRY_COMPARE(m_app->size(), QSize(2000, 1250));
    auto *contexts = qobject_cast<ContextManager *>(
        m_app->rootContext()->contextProperty("contextManager").value<QObject *>());
    QVERIFY(contexts);
    contexts->switchToContext("VC");

    QLCFixtureDefCache *cache = m_app->doc()->fixtureDefCache();
    if (cache->fixtureDef("Generic", "Generic RGB") == nullptr)
    {
        for (const QString &path : { QString(INTERNAL_FIXTUREDIR), QString("../../../../resources/fixtures/"),
                                     QString("../../../../../resources/fixtures/") })
            if (cache->loadMap(QDir(path)))
                break;
    }
    QVERIFY(cache->fixtureDef("Generic", "Generic RGB") != nullptr);

    QFile metrics(shotsDir().filePath("metrics.tsv"));
    QVERIFY(metrics.open(QIODevice::WriteOnly | QIODevice::Truncate));
    metrics.write("shot\tfixtures\tcontrols\tframes\tfunctions\tsize\tfits1080pAtPd4.9\tstructure\tcaptionMisfits\t"
                  "known%\treachContinuous%\treachAny%\tnotes\n");
}

void PagePlanner_Test::init()
{
    VirtualConsole *vc = m_app->virtualConsole();
    while (vc->pagesCount() < 8)
        vc->addPage(vc->pagesCount());
    QVERIFY(m_app->newWorkspace());
    m_app->doc()->masterTimer()->stop();
    // the DMX checks of the previous row leave their values in the universes
    m_app->doc()->inputOutputMap()->resetUniverses();
    m_app->doc()->monitorProperties()->setPointOfView(MonitorProperties::FrontView);
    for (QObject *object : m_app->rootObject()->findChildren<QObject *>())
        if (object->inherits("QQuickPopup") && object->property("visible").toBool())
            QMetaObject::invokeMethod(object, "close");
    vc->setSelectedPage(0);
    vc->setEditMode(false);
    m_nextAddress = 0;
}

void PagePlanner_Test::cleanupTestCase()
{
    m_app.reset();
}

Fixture *PagePlanner_Test::addFixture(const QString &manufacturer, const QString &model, const QString &mode)
{
    Doc *doc = m_app->doc();
    auto *fixture = new Fixture(doc);
    QLCFixtureDef *def = nullptr;
    QLCFixtureMode *fixtureMode = nullptr;
    if (model == "Generic Dimmer")
    {
        def = fixture->genericDimmerDef(mode.toInt());
        fixtureMode = fixture->genericDimmerMode(def, mode.toInt());
    }
    else
    {
        def = doc->fixtureDefCache()->fixtureDef(manufacturer, model);
        fixtureMode = def ? def->mode(mode) : nullptr;
    }
    if (fixtureMode == nullptr)
    {
        delete fixture;
        return nullptr;
    }
    fixture->setFixtureDefinition(def, fixtureMode);
    fixture->setName(QString("%1 %2").arg(model).arg(doc->fixtures().size() + 1));
    const quint32 universe = m_nextAddress / 512;
    if ((m_nextAddress % 512) + fixtureMode->channels().size() > 512)
        m_nextAddress = (universe + 1) * 512;
    fixture->setUniverse(m_nextAddress / 512);
    fixture->setAddress(m_nextAddress % 512);
    m_nextAddress += fixtureMode->channels().size();
    if (!doc->addFixture(fixture))
    {
        delete fixture;
        return nullptr;
    }
    return fixture;
}

QList<RigGroup> PagePlanner_Test::buildRig(const QString &rig)
{
    struct Part { QString group; QString manufacturer, model, mode; int count; };
    // Groups that hit the verified Stage Wizard gaps
    const QList<Part> gaps = {
        { "RGB", "Generic", "Generic RGB", "RGB", 2 },
        { "Hex", "AVE", "StagePar Hex18", "10 Channel", 2 },
        { "Spots", "Chauvet", "Intimidator Beam 140SR V2", "14 Channel", 2 },
        { "Back Spots", "Chauvet", "Intimidator Beam 140SR V2", "14 Channel", 2 },
        { "Gobo mix", "Chauvet", "Intimidator Beam 140SR V2", "14 Channel", 1 },
        { "Gobo mix", "Robe", "Robin T1 Profile", "3", 1 },
    };
    const QList<Part> main = {
        { "Pars", "American DJ", "Mega Flat TRI PAK Plus", "6 Channel", 4 },
        { "Beams", "Chauvet", "Intimidator Beam 140SR V2", "14 Channel", 4 },
        { "Washes", "Chauvet", "Intimidator Wash Zoom 350 IRC", "29-Ch", 2 },
        { "Matrix", "American DJ", "Dotz Matrix", "52 Channel", 1 },
        { "Mixed", "American DJ", "Mega Flat TRI PAK Plus", "6 Channel", 2 },
        { "Mixed", "AVE", "StagePar Hex18", "10 Channel", 2 },
    };
    QList<RigGroup> groups;
    for (const Part &part : rig == "gaps" ? gaps : main)
    {
        if (groups.isEmpty() || groups.last().name != part.group)
            groups.append({ part.group, {} });
        for (int i = 0; i < part.count; ++i)
        {
            Fixture *fixture = addFixture(part.manufacturer, part.model, part.mode);
            if (fixture == nullptr)
                qFatal("No definition %s / %s / %s", qPrintable(part.manufacturer), qPrintable(part.model),
                       qPrintable(part.mode));
            groups.last().fixtures.append(fixture->id());
        }
    }
    return groups;
}

void PagePlanner_Test::shoot(const QString &name, int page)
{
    VirtualConsole *vc = m_app->virtualConsole();
    // Widgets made from C++ render when their page is shown again
    vc->setSelectedPage(page == 0 ? 1 : 0);
    QTest::qWait(50);
    vc->setSelectedPage(page);
    QTest::qWait(400);
    const QImage image = m_app->grabWindow().convertToFormat(QImage::Format_RGB32);
    QVERIFY(image.save(shotsDir().filePath(name + ".png")));
}

void PagePlanner_Test::settle()
{
    // the Tardis thread records queued actions; a pause starts a new undo step
    Tardis *tardis = Tardis::instance();
    QTRY_VERIFY(([tardis]() {
        QMutexLocker locker(&tardis->m_queueMutex);
        return tardis->m_actionsQueue.isEmpty();
    })());
    QTest::qWait(200);
}

void PagePlanner_Test::buildPaged(const QString &name, const PagePlanner::Page &page, int first,
                                  QList<VCWidget *> &roots, int &used)
{
    VirtualConsole *vc = m_app->virtualConsole();
    // widgets are sized in pixel density units: a real 1080p screen makes them this much larger
    const qreal grow = 4.9 / vc->pixelDensity();
    // the console area of a 1920x1080 window, in this console's units
    const QRectF area(8, 8, 1880 / grow, 960 / grow);
    PagePlanner::Page rest = page;
    used = 0;
    while (!rest.sections.isEmpty())
    {
        while (vc->pagesCount() <= first + used)
            vc->addPage(vc->pagesCount());
        const PageBuilder::Result result = PageBuilder::build(vc, m_app->doc(), vc->page(first + used), rest, area);
        QVERIFY(result.sections > 0);
        QVERIFY2(result.size.width() * grow <= 1920 && result.size.height() * grow <= 1080,
                 qPrintable(QString("%1 part %2 is %3x%4 at pd 4.9").arg(name).arg(used + 1)
                            .arg(qRound(result.size.width() * grow)).arg(qRound(result.size.height() * grow))));
        roots.append(result.frames);
        const bool whole = used == 0 && result.sections == rest.sections.size();
        shoot(whole ? name : QString("%1-%2").arg(name).arg(used + 1), first + used);
        rest.sections = rest.sections.mid(result.sections);
        used++;
    }
}

void PagePlanner_Test::measure(const QString &name, const QList<quint32> &fixtures,
                               const QList<VCWidget *> &roots, int subpage, int functions)
{
    Doc *doc = m_app->doc();
    PageStats stats;
    QRectF bounds;
    for (VCWidget *root : roots)
    {
        VCFrame *frame = qobject_cast<VCFrame *>(root);
        if (subpage >= 0 && frame)
        {
            // Only one subpage of a multipage frame: its own children
            QList<VCWidget *> shown;
            for (VCWidget *child : frame->children(false))
            {
                if (child->page() != subpage)
                    continue;
                collect(doc, child, stats);
                bounds |= child->logicalGeometry().translated(frame->logicalGeometry().topLeft());
                for (VCWidget *other : shown)
                {
                    const QRectF overlap = other->logicalGeometry().intersected(child->logicalGeometry());
                    if (overlap.width() > 0.01 && overlap.height() > 0.01)
                    {
                        stats.structureErrors++;
                        stats.notes << QString("'%1' overlaps '%2'").arg(child->caption(), other->caption());
                    }
                }
                shown.append(child);
            }
            stats.frames++;
            continue;
        }
        collect(doc, root, stats);
        bounds |= root->logicalGeometry();
    }
    for (int i = 0; i < roots.size(); i++)
        for (int j = i + 1; j < roots.size(); j++)
        {
            const QRectF overlap = roots[i]->logicalGeometry().intersected(roots[j]->logicalGeometry());
            if (overlap.width() > 0.01 && overlap.height() > 0.01)
                stats.structureErrors++;
        }

    // A control is reached when every coarse channel of it is: fine
    // channels ride with their coarse channel
    int total = 0, continuous = 0, any = 0;
    for (quint32 id : fixtures)
    {
        const Fixture *fixture = doc->fixture(id);
        for (const FixtureControls::Control &control : FixtureControls::analyze(fixture->fixtureMode()))
        {
            bool byFader = true, byAny = true;
            for (quint32 ch : control.channels)
            {
                if (fixture->channel(ch)->controlByte() != QLCChannel::MSB)
                    continue;
                const QPair<quint32, quint32> key(id, ch);
                byFader = byFader && stats.continuous.contains(key);
                byAny = byAny && (stats.continuous.contains(key) || stats.fixed.contains(key));
            }
            total += control.channels.size();
            continuous += byFader ? control.channels.size() : 0;
            any += byAny ? control.channels.size() : 0;
        }
    }

    // widgets scale with the pixel density: a real 1080p screen gives 4.9, the harness less
    const qreal grow = 4.9 / m_app->virtualConsole()->pixelDensity();
    const bool fits = bounds.right() * grow <= 1920 && bounds.bottom() * grow <= 1080;
    const QString row = QString("%1\t%2\t%3\t%4\t%5\t%6x%7\t%8\t%9\t%10\t%11\t%12\t%13\t%14\n")
        .arg(name).arg(fixtures.size()).arg(stats.controls).arg(stats.frames).arg(functions)
        .arg(qRound(bounds.right())).arg(qRound(bounds.bottom())).arg(fits ? "yes" : "no")
        .arg(stats.structureErrors).arg(stats.captionMisfits)
        .arg(stats.controls ? qRound(100.0 * stats.known / stats.controls) : 0)
        .arg(total ? qRound(100.0 * continuous / total) : 0)
        .arg(total ? qRound(100.0 * any / total) : 0)
        .arg(stats.notes.join("; "));
    m_lastPage = { stats.structureErrors, stats.pads, total ? qRound(100.0 * any / total) : 0, stats.misfitCaptions };
    QFile metrics(shotsDir().filePath("metrics.tsv"));
    QVERIFY(metrics.open(QIODevice::Append));
    metrics.write(row.toUtf8());
    qInfo().noquote() << row.trimmed();
}

static void writeSection(QTextStream &out, const PagePlanner::Section &section, const QString &level)
{
    static const char *kinds[] = { "Fader", "Presets", "ColorMixer", "PanTilt" };
    out << level << " " << section.title << (section.strip ? " (strip)" : "") << "\n";
    for (const PagePlanner::Slot &slot : section.items)
        out << "- " << slot.label << " [" << slot.key << ", " << kinds[slot.kind] << ", "
            << slot.channels.size() << " ch, " << slot.heads.size() << " heads, "
            << slot.choices.size() << " choices]\n";
    for (const PagePlanner::Section &part : section.parts)
        writeSection(out, part, level + "#");
}

/* The planned page as text, beside its screenshot */
static void writePlan(const QString &name, const QList<PagePlanner::Page> &pages)
{
    QFile file(shotsDir().filePath(name + ".txt"));
    if (!file.open(QIODevice::WriteOnly))
        return;
    QTextStream out(&file);
    for (const PagePlanner::Page &page : pages)
    {
        out << "# " << page.title << "\n";
        for (const PagePlanner::Section &section : page.sections)
            writeSection(out, section, "##");
    }
}

static void collectDuplicates(const QString &where, const PagePlanner::Section &section, QStringList &out)
{
    QSet<QString> seen;
    for (const PagePlanner::Slot &slot : section.items)
    {
        if (seen.contains(slot.label))
            out << QString("%1 / %2: %3").arg(where, section.title, slot.label);
        seen.insert(slot.label);
    }
    for (const PagePlanner::Section &part : section.parts)
        collectDuplicates(where, part, out);
}

/* Captions that appear twice in one section: two controls nobody can tell apart */
static QStringList duplicateCaptions(const QList<PagePlanner::Page> &pages)
{
    QStringList out;
    for (const PagePlanner::Page &page : pages)
        for (const PagePlanner::Section &section : page.sections)
            collectDuplicates(page.title, section, out);
    return out;
}

static void collectLayoutErrors(const QString &where, const PagePlanner::Section &section, QStringList &out)
{
    static const QStringList noOps = { "no function", "no effect", "nothing", "none", "unused" };
    for (int i = 0; i < section.items.size(); ++i)
    {
        const PagePlanner::Slot &slot = section.items.at(i);
        // a speed fader sits right after the buttons whose ranges it sets
        if (slot.key.endsWith("/rate") && (i == 0 || section.items.at(i - 1).choices.isEmpty()))
            out << QString("%1 / %2: '%3' is not beside its buttons").arg(where, section.title, slot.label);
        for (const PagePlanner::Choice &choice : slot.choices)
            if (noOps.contains(choice.label.trimmed().toLower()))
                out << QString("%1 / %2: button '%3'").arg(where, slot.label, choice.label);
    }
    for (const PagePlanner::Section &part : section.parts)
        collectLayoutErrors(where, part, out);
}

/* Speed faders away from their buttons, buttons named after a fixture's "No function" */
static QStringList layoutErrors(const QList<PagePlanner::Page> &pages)
{
    QStringList out;
    for (const PagePlanner::Page &page : pages)
        for (const PagePlanner::Section &section : page.sections)
            collectLayoutErrors(page.title, section, out);
    return out;
}

void PagePlanner_Test::presetRule_data()
{
    QTest::addColumn<QString>("manufacturer");
    QTest::addColumn<QString>("model");
    QTest::addColumn<QString>("mode");
    QTest::addColumn<QString>("channel");
    // the widgets of the channel: buttons (capability or swatch buttons), fader, picker (Click & Go)
    QTest::addColumn<QString>("widgets");

    QTest::newRow("beam shutter") << "Chauvet" << "Intimidator Beam 140SR V2" << "14 Channel" << "Shutter" << "buttons+fader";
    QTest::newRow("beam prism") << "Chauvet" << "Intimidator Beam 140SR V2" << "14 Channel" << "Prism" << "buttons+fader";
    QTest::newRow("profile prism rotation") << "Robe" << "Robin T1 Profile" << "3" << "Prism Rotation" << "buttons+fader";
    QTest::newRow("profile animation rotation") << "Robe" << "Robin T1 Profile" << "3" << "Animation Rot" << "buttons+fader";
    QTest::newRow("beam colour wheel") << "Chauvet" << "Intimidator Beam 140SR V2" << "14 Channel" << "Color Wheel" << "buttons+picker";
    QTest::newRow("beam gobo wheel") << "Chauvet" << "Intimidator Beam 140SR V2" << "14 Channel" << "Gobo Wheel" << "buttons+picker";
    QTest::newRow("beam macros") << "Chauvet" << "Intimidator Beam 140SR V2" << "14 Channel" << "Movement Macros" << "picker";
    QTest::newRow("tri par shutter") << "American DJ" << "Mega Flat TRI PAK Plus" << "6 Channel" << "Shutter/Strobe" << "buttons+fader";
}

void PagePlanner_Test::presetRule()
{
    QFETCH(QString, manufacturer);
    QFETCH(QString, model);
    QFETCH(QString, mode);
    QFETCH(QString, channel);
    QFETCH(QString, widgets);
    Fixture *fixture = addFixture(manufacturer, model, mode);
    QVERIFY(fixture);
    quint32 index = QLCChannel::invalid();
    for (quint32 i = 0; i < fixture->channels(); ++i)
        if (fixture->channel(i)->name() == channel)
            index = i;
    QVERIFY(index != QLCChannel::invalid());

    bool buttons = false, fader = false, picker = false;
    std::function<void(const PagePlanner::Section &)> visit = [&](const PagePlanner::Section &section) {
        for (const PagePlanner::Slot &slot : section.items)
        {
            for (const PagePlanner::Choice &choice : slot.choices)
                for (const PagePlanner::Value &value : choice.values)
                    buttons = buttons || (value.fixture == fixture->id() && value.channel == index);
            if (!slot.channels.contains({ fixture->id(), index }))
                continue;
            fader = fader || slot.kind == FixtureControls::Fader;
            picker = picker || slot.kind == FixtureControls::Presets;
        }
        for (const PagePlanner::Section &part : section.parts)
            visit(part);
    };
    for (const PagePlanner::Page &page : PagePlanner::plan(m_app->doc(), { fixture->id() }, fixture->name(), {}))
        for (const PagePlanner::Section &section : page.sections)
            visit(section);
    QStringList found;
    if (buttons)
        found << "buttons";
    if (fader)
        found << "fader";
    if (picker)
        found << "picker";
    QCOMPARE(found.join('+'), widgets);
}

void PagePlanner_Test::choiceLabels_data()
{
    QTest::addColumn<QString>("manufacturer");
    QTest::addColumn<QString>("model");
    QTest::addColumn<QString>("mode");
    QTest::addColumn<QString>("channel");
    QTest::addColumn<QStringList>("labels");   // of the buttons of the channel

    // "No function" of a strobe or prism channel turns the effect off
    QTest::newRow("hex par strobe") << "AVE" << "StagePar Hex18" << "10 Channel" << "Strobe"
                                    << QStringList({ "Off", "Strobe from slow to fast" });
    QTest::newRow("beam prism") << "Chauvet" << "Intimidator Beam 140SR V2" << "14 Channel" << "Prism"
                                << QStringList({ "Off", "7-facet prism", "Rotation, slow to fast",
                                                 "Reverse rotation, slow to fast" });
}

void PagePlanner_Test::choiceLabels()
{
    QFETCH(QString, manufacturer);
    QFETCH(QString, model);
    QFETCH(QString, mode);
    QFETCH(QString, channel);
    QFETCH(QStringList, labels);
    Fixture *fixture = addFixture(manufacturer, model, mode);
    QVERIFY(fixture);
    quint32 index = QLCChannel::invalid();
    for (quint32 i = 0; i < fixture->channels(); ++i)
        if (fixture->channel(i)->name() == channel)
            index = i;
    QVERIFY(index != QLCChannel::invalid());

    QStringList found;
    const QList<PagePlanner::Page> pages = PagePlanner::plan(m_app->doc(), { fixture->id() }, fixture->name(), {});
    for (const PagePlanner::Section &section : pages.first().sections)
        for (const PagePlanner::Slot &slot : section.items)
            for (const PagePlanner::Choice &choice : slot.choices)
                if (!choice.values.isEmpty() && choice.values.first().channel == index)
                    found << choice.label.trimmed();
    QCOMPARE(found, labels);
}

void PagePlanner_Test::channelSection_data()
{
    QTest::addColumn<QString>("manufacturer");
    QTest::addColumn<QString>("model");
    QTest::addColumn<QString>("mode");
    QTest::addColumn<QString>("channel");
    QTest::addColumn<QString>("section");

    // a speed goes with what it moves
    QTest::newRow("profile p/t speed") << "Robe" << "Robin T1 Profile" << "3" << "P/T Speed, P/T Time" << "Position";
    QTest::newRow("profile gobo speed") << "Robe" << "Robin T1 Profile" << "3" << "Gobo Speed" << "Gobo";
    QTest::newRow("profile beam time") << "Robe" << "Robin T1 Profile" << "3" << "Beam Time" << "Beam";
    QTest::newRow("beam pan/tilt speed") << "Chauvet" << "Intimidator Beam 140SR V2" << "14 Channel" << "Pan/Tilt Speed" << "Position";
    QTest::newRow("hex par program speed") << "AVE" << "StagePar Hex18" << "10 Channel" << "Auto/Sound speed" << "Effects";
}

void PagePlanner_Test::channelSection()
{
    QFETCH(QString, manufacturer);
    QFETCH(QString, model);
    QFETCH(QString, mode);
    QFETCH(QString, channel);
    QFETCH(QString, section);
    Fixture *fixture = addFixture(manufacturer, model, mode);
    QVERIFY(fixture);
    quint32 index = QLCChannel::invalid();
    for (quint32 i = 0; i < fixture->channels(); ++i)
        if (fixture->channel(i)->name() == channel)
            index = i;
    QVERIFY(index != QLCChannel::invalid());

    QStringList found;
    const QList<PagePlanner::Page> pages = PagePlanner::plan(m_app->doc(), { fixture->id() }, fixture->name(), {});
    for (const PagePlanner::Section &s : pages.first().sections)
        for (const PagePlanner::Slot &slot : s.items)
            if (slot.channels.contains({ fixture->id(), index }))
                found << s.title;
    QCOMPARE(found, QStringList({ section }));
}

void PagePlanner_Test::sectionOrder_data()
{
    QTest::addColumn<QString>("manufacturer");
    QTest::addColumn<QString>("model");
    QTest::addColumn<QString>("mode");
    QTest::addColumn<QString>("sections");

    // light first (intensity and shutter decide whether light comes out), then where, colour, beam shaping
    QTest::newRow("beam") << "Chauvet" << "Intimidator Beam 140SR V2" << "14 Channel"
                          << "Intensity,Position,Color,Gobo,Beam,Effects,Setup";
    QTest::newRow("tri par") << "American DJ" << "Mega Flat TRI PAK Plus" << "6 Channel" << "Intensity,Color";
    QTest::newRow("hazer") << "Chauvet" << "Hurricane Haze 2" << "Generic" << "Output,Fan";
}

void PagePlanner_Test::sectionOrder()
{
    QFETCH(QString, manufacturer);
    QFETCH(QString, model);
    QFETCH(QString, mode);
    QFETCH(QString, sections);
    Fixture *fixture = addFixture(manufacturer, model, mode);
    QVERIFY(fixture);
    const QList<PagePlanner::Page> pages = PagePlanner::plan(m_app->doc(), { fixture->id() }, fixture->name(), {});
    QStringList titles;
    for (const PagePlanner::Section &section : pages.first().sections)
        titles << section.title;
    QCOMPARE(titles.join(','), sections);
}

namespace
{
/** Every string translated: "Color" becomes "[Color]" */
class BracketTranslator : public QTranslator
{
public:
    QString translate(const char *, const char *sourceText, const char *, int) const override
    {
        return QString("[%1]").arg(QString::fromUtf8(sourceText));
    }
    bool isEmpty() const override { return false; }
};
}

void PagePlanner_Test::sectionOrderTranslated()
{
    Fixture *fixture = addFixture("Chauvet", "Intimidator Beam 140SR V2", "14 Channel");
    QVERIFY(fixture);
    BracketTranslator translator;
    QCoreApplication::installTranslator(&translator);
    const QList<PagePlanner::Page> pages = PagePlanner::plan(m_app->doc(), { fixture->id() }, fixture->name(), {});
    QCoreApplication::removeTranslator(&translator);
    QStringList titles;
    for (const PagePlanner::Section &section : pages.first().sections)
        titles << section.title;
    QCOMPARE(titles.join(','), QString("[Intensity],[Position],[Color],[Gobo],[Beam],[Effects],[Setup]"));
}

void PagePlanner_Test::libraryReach()
{
    // every mode of the library: each coarse channel that does something is
    // in a widget of its page (a fader, a picker, a pad or a button value)
    Doc *doc = m_app->doc();
    int modes = 0;
    QStringList misses;
    QLCFixtureDefCache *cache = doc->fixtureDefCache();
    QList<QLCFixtureDef *> defs;
    for (const QString &manufacturer : cache->manufacturers())
        for (const QString &model : cache->models(manufacturer))
            if (QLCFixtureDef *def = cache->fixtureDef(manufacturer, model))
                defs.append(def);
    for (QLCFixtureDef *def : defs)
    {
        for (QLCFixtureMode *mode : def->modes())
        {
            if (mode->channels().size() > 512)
                continue;
            auto *fixture = new Fixture(doc);
            fixture->setName(def->model());
            fixture->setFixtureDefinition(def, mode);
            fixture->setAddress(0);
            fixture->setUniverse(0);
            if (!doc->addFixture(fixture))
            {
                delete fixture;
                continue;
            }
            modes++;
            QSet<quint32> reached;
            std::function<void(const PagePlanner::Section &)> visit = [&](const PagePlanner::Section &section) {
                for (const PagePlanner::Slot &slot : section.items)
                {
                    for (const PagePlanner::Channel &ch : slot.channels)
                        reached.insert(ch.channel);
                    for (const PagePlanner::Choice &choice : slot.choices)
                        for (const PagePlanner::Value &value : choice.values)
                            reached.insert(value.channel);
                    if (slot.kind == FixtureControls::PanTilt)
                        for (const PagePlanner::Head &head : slot.heads)
                            for (int type : { int(QLCChannel::Pan), int(QLCChannel::Tilt) })
                                reached.insert(fixture->channelNumber(type, QLCChannel::MSB, head.head));
                }
                for (const PagePlanner::Section &part : section.parts)
                    visit(part);
            };
            for (const PagePlanner::Page &page : PagePlanner::plan(doc, { fixture->id() }, fixture->name(), {}))
                for (const PagePlanner::Section &section : page.sections)
                    visit(section);
            for (quint32 i = 0; i < fixture->channels(); ++i)
            {
                const QLCChannel *ch = fixture->channel(i);
                if (ch->group() == QLCChannel::Nothing || ch->controlByte() != QLCChannel::MSB || reached.contains(i))
                    continue;
                misses << QString("%1 / %2 / %3: %4").arg(def->manufacturer(), def->model(), mode->name(), ch->name());
            }
            QVERIFY(doc->deleteFixture(fixture->id()));
        }
    }
    qInfo() << "modes" << modes << "unreached channels" << misses.size();
    for (const QString &miss : misses.mid(0, 20))
        qInfo().noquote() << miss;
    QVERIFY(modes > 5000);
    QVERIFY2(misses.isEmpty(), qPrintable(QString::number(misses.size())));
}

void PagePlanner_Test::referencePage_data()
{
    QTest::addColumn<QString>("manufacturer");
    QTest::addColumn<QString>("model");
    QTest::addColumn<QString>("mode");
    QTest::newRow("01-generic-rgb") << "Generic" << "Generic RGB" << "RGB Dimmer";
    QTest::newRow("02-tri-par") << "American DJ" << "Mega Flat TRI PAK Plus" << "6 Channel";
    QTest::newRow("03-hex-par") << "AVE" << "StagePar Hex18" << "10 Channel";
    QTest::newRow("04-beam") << "Chauvet" << "Intimidator Beam 140SR V2" << "14 Channel";
    QTest::newRow("05-profile") << "Robe" << "Robin T1 Profile" << "3";
    QTest::newRow("06-wash-heads") << "Chauvet" << "Intimidator Wash Zoom 350 IRC" << "29-Ch";
    QTest::newRow("07-matrix") << "American DJ" << "Dotz Matrix" << "52 Channel";
    QTest::newRow("08-pixel-160") << "Blizzard Lighting" << "Pixellicious" << "480 Channel";
    QTest::newRow("09-strobe") << "Chauvet" << "DMX Mega Strobe III" << "Standard";
    QTest::newRow("10-hazer") << "Chauvet" << "Hurricane Haze 2" << "Generic";
    QTest::newRow("11-dimmer-pack") << "Generic" << "Generic Dimmer" << "4";
}

void PagePlanner_Test::referencePage()
{
    QFETCH(QString, manufacturer);
    QFETCH(QString, model);
    QFETCH(QString, mode);
    Fixture *fixture = addFixture(manufacturer, model, mode);
    QVERIFY2(fixture, qPrintable(QString("No definition %1 / %2 / %3").arg(manufacturer, model, mode)));

    const QString name = QString::fromLatin1(QTest::currentDataTag());
    const QList<PagePlanner::Page> pages = PagePlanner::plan(m_app->doc(), { fixture->id() }, fixture->name(), {});
    writePlan(name, pages);
    const int before = m_app->doc()->functions().size();
    QList<VCWidget *> roots;
    int used = 0;
    buildPaged(name, pages.first(), 0, roots, used);
    if (QTest::currentTestFailed())
        return;
    QVERIFY(!roots.isEmpty());
    measure(name, { fixture->id() }, roots, -1, m_app->doc()->functions().size() - before);
    QVERIFY2(m_lastPage.misfitCaptions.isEmpty(), qPrintable(m_lastPage.misfitCaptions.join("; ")));
    const QStringList duplicates = duplicateCaptions(pages);
    QVERIFY2(duplicates.isEmpty(), qPrintable(duplicates.join("; ")));
    const QStringList misplaced = layoutErrors(pages);
    QVERIFY2(misplaced.isEmpty(), qPrintable(misplaced.join("; ")));

    bool panTilt = false;
    for (const FixtureControls::Control &control : FixtureControls::analyze(fixture->fixtureMode()))
        panTilt = panTilt || control.kind == FixtureControls::PanTilt;
    QCOMPARE(m_lastPage.structureErrors, 0);
    QCOMPARE(m_lastPage.pads > 0, panTilt);
    QCOMPARE(m_lastPage.reachAny, 100);

    // a strobe gets one Flash button that puts its dimmers at full
    QList<VCButton *> flashes;
    QList<VCButton *> allButtons;
    for (VCWidget *root : roots)
        collectWidgets(root, allButtons);
    for (VCButton *button : allButtons)
        if (button->actionType() == VCButton::Flash)
            flashes.append(button);
    if (fixture->fixtureDef()->type() == QLCFixtureDef::Strobe)
    {
        QCOMPARE(flashes.size(), 1);
        const Scene *scene = qobject_cast<const Scene *>(m_app->doc()->function(flashes.first()->functionID()));
        QVERIFY(scene);
        for (quint32 i = 0; i < fixture->channels(); ++i)
        {
            const QLCChannel *ch = fixture->channel(i);
            if (ch->group() == QLCChannel::Intensity && ch->colour() == QLCChannel::NoColour &&
                ch->controlByte() == QLCChannel::MSB)
                QCOMPARE(int(scene->value(fixture->id(), i)), 255);
        }
    }
    else
        QCOMPARE(flashes.size(), 0);

    // a pixel fixture (several colour mixers, no pan/tilt) gets an RGB matrix over all its heads
    int mixers = 0;
    for (const FixtureControls::Control &control : FixtureControls::analyze(fixture->fixtureMode()))
        mixers += control.kind == FixtureControls::ColorMixer && control.head >= 0 ? 1 : 0;
    QList<VCAnimation *> animations;
    for (VCWidget *root : roots)
        collectWidgets(root, animations);
    if (mixers >= 2 && !panTilt)
    {
        QCOMPARE(animations.size(), 1);
        const RGBMatrix *matrix = qobject_cast<const RGBMatrix *>(m_app->doc()->function(animations.first()->functionID()));
        QVERIFY(matrix);
        const FixtureGroup *group = m_app->doc()->fixtureGroup(matrix->fixtureGroup());
        QVERIFY(group);
        QCOMPARE(group->headList().size(), fixture->heads());
    }
    else
        QCOMPARE(animations.size(), 0);

    // per-head frames follow the physical layout: as many columns as the layout is wide
    const QSize layout = fixture->fixtureMode()->physical().layoutSize();
    if (layout.width() * layout.height() == fixture->heads() && fixture->heads() > 1)
    {
        QList<VCFrame *> frames;
        for (VCWidget *root : roots)
            collectWidgets(root, frames);
        for (VCFrame *frame : frames)
        {
            if (!frame->caption().endsWith(QObject::tr(" per head")))
                continue;
            QSet<int> columns;
            for (VCWidget *child : frame->children(false))
                columns.insert(qRound(child->logicalGeometry().x()));
            QVERIFY2(columns.size() == layout.width(), qPrintable(QString("'%1' has %2 columns, layout %3x%4")
                     .arg(frame->caption()).arg(columns.size()).arg(layout.width()).arg(layout.height())));
        }
    }

    // every pad moves the fixture's pan and tilt: two positions, two outputs
    QList<VCXYPad *> pads;
    for (VCWidget *root : roots)
        collectWidgets(root, pads);
    const QLCFixtureMode *fixtureMode = fixture->fixtureMode();
    const auto written = [&](DMXSource *source) {
        const QList<Universe *> universes = m_app->doc()->inputOutputMap()->claimUniverses();
        Universe *universe = universes.at(int(fixture->universe()));
        source->writeDMX(m_app->doc()->masterTimer(), universes);
        UniverseFaders::process(universe);
        const QByteArray values = universe->preGMValues();
        m_app->doc()->inputOutputMap()->releaseUniverses(false);
        return values;
    };
    for (VCXYPad *pad : pads)
    {
        pad->setCurrentPosition(QPointF(200, 60));
        const QByteArray first = written(pad);
        pad->setCurrentPosition(QPointF(60, 200));
        const QByteArray second = written(pad);
        for (QLCChannel::Group group : { QLCChannel::Pan, QLCChannel::Tilt })
        {
            const quint32 channel = fixtureMode->channelNumber(group);
            QVERIFY(channel != QLCChannel::invalid());
            const int at = int(fixture->address() + channel);
            QVERIFY2(first.at(at) != second.at(at),
                     qPrintable(QString("%1 channel %2 stays %3").arg(QLCChannel::groupToString(group))
                                .arg(channel).arg(uchar(first.at(at)))));
        }
    }

    // red, green, blue and white+amber+UV picks on every colour slider: the
    // picked emitters full, the others off whatever their order (Lime and
    // Indigo are not part of a Click & Go colour)
    QList<VCSlider *> sliders;
    for (VCWidget *root : roots)
        collectWidgets(root, sliders);
    // every button runs a scene; one named like a capability of its channel
    // sets a value inside a capability of that name (a name can cover several ranges)
    QList<VCButton *> buttons;
    for (VCWidget *root : roots)
        collectWidgets(root, buttons);
    for (VCButton *button : buttons)
    {
        const Scene *scene = qobject_cast<const Scene *>(m_app->doc()->function(button->functionID()));
        QVERIFY2(scene && !scene->values().isEmpty(), qPrintable(button->caption()));
        for (const SceneValue &value : scene->values())
        {
            bool named = false, inside = false;
            for (const QLCCapability *cap : fixture->channel(value.channel)->capabilities())
            {
                if (cap->name().compare(button->caption(), Qt::CaseInsensitive) != 0)
                    continue;
                named = true;
                inside = inside || (value.value >= cap->min() && value.value <= cap->max());
            }
            QVERIFY2(!named || inside, qPrintable(QString("'%1' sets %2").arg(button->caption()).arg(value.value)));
        }
    }

    // every plain level fader drives its channels
    for (VCSlider *slider : sliders)
    {
        if (slider->sliderMode() != VCSlider::Level || slider->clickAndGoType() != VCSlider::CnGNone)
            continue;
        slider->setValue(200);
        const QByteArray values = written(slider);
        for (const SceneValue &value : slider->levelChannels())
            QVERIFY2(uchar(values.at(int(fixture->address() + value.channel))) == 200,
                     qPrintable(QString("'%1' leaves %2 at %3").arg(slider->caption(), fixture->channel(value.channel)->name())
                                .arg(uchar(values.at(int(fixture->address() + value.channel))))));
        slider->setValue(0);
        written(slider);
    }

    const QList<QPair<QColor, QColor>> picks = {
        { Qt::red, Qt::black }, { Qt::green, Qt::black }, { Qt::blue, Qt::black }, { Qt::black, Qt::white } };
    for (VCSlider *slider : sliders)
    {
        if (slider->clickAndGoType() != VCSlider::CnGColors)
            continue;
        for (const QPair<QColor, QColor> &pick : picks)
        {
            slider->setClickAndGoColors(pick.first, pick.second);
            const QByteArray values = written(slider);
            for (const SceneValue &value : slider->levelChannels())
            {
                const QLCChannel *channel = fixture->channel(value.channel);
                int expected = -1;
                switch (channel->colour())
                {
                    case QLCChannel::Red: expected = pick.first.red(); break;
                    case QLCChannel::Green: expected = pick.first.green(); break;
                    case QLCChannel::Blue: expected = pick.first.blue(); break;
                    case QLCChannel::White: expected = pick.second.red(); break;
                    case QLCChannel::Amber: expected = pick.second.green(); break;
                    case QLCChannel::UV: expected = pick.second.blue(); break;
                    case QLCChannel::Lime: case QLCChannel::Indigo: expected = 0; break;
                    default: break;
                }
                const int actual = uchar(values.at(int(fixture->address() + value.channel)));
                if (expected >= 0)
                    QVERIFY2(actual == expected, qPrintable(QString("%1 is %2 after a %3 + %4 pick")
                                                            .arg(channel->name()).arg(actual)
                                                            .arg(pick.first.name(), pick.second.name())));
            }
        }
        // back to 0, or its last pick holds the channels of the next slider (HTP)
        slider->setValue(0);
        written(slider);
    }
}

void PagePlanner_Test::wizardRig_data()
{
    QTest::addColumn<QString>("rig");
    QTest::newRow("main") << "main";
    QTest::newRow("gaps") << "gaps";
}

void PagePlanner_Test::wizardRig()
{
    QFETCH(QString, rig);
    Doc *doc = m_app->doc();
    const QList<RigGroup> groups = buildRig(rig);
    auto *fixtureManager = m_app->rootContext()->contextProperty("fixtureManager").value<FixtureManager *>();
    auto *functionManager = m_app->rootContext()->contextProperty("functionManager").value<FunctionManager *>();
    auto *contexts = m_app->rootContext()->contextProperty("contextManager").value<ContextManager *>();
    VirtualConsole *vc = m_app->virtualConsole();
    QVERIFY(fixtureManager && functionManager && contexts);

    StageWizard wizard(doc, fixtureManager, functionManager, vc, contexts);
    for (const RigGroup &group : groups)
    {
        const int index = wizard.addGroup();
        wizard.renameGroup(index, group.name);
        for (quint32 id : group.fixtures)
            wizard.assignFixtureToGroup(index, id);
        wizard.setGroupSelected(index, true);
    }
    wizard.autoDetectRoles();
    // the effects step computes which effects the groups can use and applies the show type defaults
    wizard.setCurrentStep(3);
    // off by default, but the gobo index gap only shows with gobo scenes
    wizard.setEffectEnabled(StageWizard::EffectGoboPalette, true);
    wizard.setEffectEnabled(StageWizard::EffectPrism, true);
    const int before = doc->functions().size();
    wizard.generate();
    const int functions = doc->functions().size() - before;

    VCFrame *master = nullptr;
    for (VCWidget *child : vc->page(0)->children(false))
        if (VCFrame *frame = qobject_cast<VCFrame *>(child))
            master = frame;
    QVERIFY(master);

    // Gobo scene values that select another gobo than the one the scene is named for
    int misplaced = 0, goboValues = 0;
    for (Function *function : doc->functions())
    {
        Scene *scene = qobject_cast<Scene *>(function);
        if (scene == nullptr || !scene->path().contains("/Gobos"))
            continue;
        const QString gobo = scene->name().section(QString::fromUtf8(" \u2013 "), -1);
        for (const SceneValue &value : scene->values())
        {
            const Fixture *fixture = doc->fixture(value.fxi);
            const QLCChannel *channel = fixture->channel(value.channel);
            const QLCCapability *cap = channel ? channel->searchCapability(value.value) : nullptr;
            goboValues++;
            if (channel == nullptr || channel->group() != QLCChannel::Gobo || cap == nullptr || cap->name() != gobo)
            {
                misplaced++;
                qInfo().noquote() << "gobo scene" << scene->name() << "sets" << fixture->name() << "channel"
                                  << (channel ? channel->name() : QString("none")) << "to"
                                  << (cap ? cap->name() : QString("nothing"));
            }
        }
    }
    qInfo() << "gobo values selecting another gobo:" << misplaced << "of" << goboValues;

    QList<quint32> all;
    for (const RigGroup &group : groups)
        all.append(group.fixtures);
    const QString prefix = rig == "gaps" ? "G1-wizard-gaps" : "G1-wizard";
    for (int p = 0; p < master->totalPagesNumber(); ++p)
    {
        const QString slug = p == 0 ? QString("all") : groups.value(p - 1).name.toLower().replace(' ', '-');
        const QString name = QString("%1-%2-%3").arg(prefix).arg(p).arg(slug);
        master->setCurrentPage(p);
        measure(name, p == 0 ? all : groups.value(p - 1).fixtures, { master }, p, functions);
        shoot(name, 0);
    }
}

void PagePlanner_Test::engineRig_data()
{
    QTest::addColumn<QString>("rig");
    QTest::addColumn<QString>("group");
    QTest::addColumn<int>("strategy");
    QTest::addColumn<int>("heads");
    QTest::newRow("G2-engine-0-all-intersection") << "main" << "" << int(PagePlanner::Intersection) << int(PagePlanner::MergeHeads);
    QTest::newRow("G2-engine-0-all-union") << "main" << "" << int(PagePlanner::Union) << int(PagePlanner::MergeHeads);
    QTest::newRow("G2-engine-0-all-master") << "main" << "" << int(PagePlanner::MasterAndModels) << int(PagePlanner::MergeHeads);
    QTest::newRow("G2-engine-1-pars") << "main" << "Pars" << int(PagePlanner::Union) << int(PagePlanner::HeadStrips);
    QTest::newRow("G2-engine-2-beams") << "main" << "Beams" << int(PagePlanner::Union) << int(PagePlanner::HeadStrips);
    QTest::newRow("G2-engine-3-washes") << "main" << "Washes" << int(PagePlanner::Union) << int(PagePlanner::HeadStrips);
    QTest::newRow("G2-engine-4-matrix-strips") << "main" << "Matrix" << int(PagePlanner::Union) << int(PagePlanner::HeadStrips);
    QTest::newRow("G2-engine-4-matrix-merged") << "main" << "Matrix" << int(PagePlanner::Union) << int(PagePlanner::MergeHeads);
    QTest::newRow("G2-engine-5-mixed-intersection") << "main" << "Mixed" << int(PagePlanner::Intersection) << int(PagePlanner::HeadStrips);
    QTest::newRow("G2-engine-5-mixed-union") << "main" << "Mixed" << int(PagePlanner::Union) << int(PagePlanner::HeadStrips);
    QTest::newRow("G2-engine-5-mixed-master") << "main" << "Mixed" << int(PagePlanner::MasterAndModels) << int(PagePlanner::HeadStrips);
    for (const QString &group : { "RGB", "Hex", "Spots", "Back Spots", "Gobo mix" })
        QTest::newRow(qPrintable("G2-engine-gaps-" + group.toLower().replace(' ', '-')))
            << "gaps" << group << int(PagePlanner::Union) << int(PagePlanner::HeadStrips);
}

void PagePlanner_Test::engineRig()
{
    QFETCH(QString, rig);
    QFETCH(QString, group);
    QFETCH(int, strategy);
    QFETCH(int, heads);
    const QList<RigGroup> groups = buildRig(rig);
    QList<quint32> fixtures;
    for (const RigGroup &g : groups)
        if (group.isEmpty() || g.name == group)
            fixtures.append(g.fixtures);

    PagePlanner::Options options;
    options.strategy = PagePlanner::Strategy(strategy);
    options.heads = PagePlanner::HeadMode(heads);
    const QString name = QString::fromLatin1(QTest::currentDataTag());
    const QList<PagePlanner::Page> pages = PagePlanner::plan(m_app->doc(), fixtures,
                                                             group.isEmpty() ? QString("All groups") : group, options);
    writePlan(name, pages);
    int next = 0;
    for (int p = 0; p < pages.size(); ++p)
    {
        const int before = m_app->doc()->functions().size();
        const QString shot = pages.size() > 1 ? QString("%1-p%2").arg(name).arg(p) : name;
        QList<VCWidget *> roots;
        int used = 0;
        buildPaged(shot, pages.at(p), next, roots, used);
        if (QTest::currentTestFailed())
            return;
        next += used;
        QVERIFY(!roots.isEmpty());
        // Every page measures against the whole group: the pages together must reach it
        measure(shot, fixtures, roots, -1, m_app->doc()->functions().size() - before);
        QVERIFY2(m_lastPage.misfitCaptions.isEmpty(), qPrintable(m_lastPage.misfitCaptions.join("; ")));
    }
    const QStringList duplicates = duplicateCaptions(pages);
    QVERIFY2(duplicates.isEmpty(), qPrintable(duplicates.join("; ")));
    const QStringList misplaced = layoutErrors(pages);
    QVERIFY2(misplaced.isEmpty(), qPrintable(misplaced.join("; ")));
}

void PagePlanner_Test::consolePageAction_data()
{
    QTest::addColumn<QStringList>("fixtures");   // manufacturer|model|mode
    QTest::addColumn<QSize>("window");
    QTest::addColumn<bool>("several");           // more than one console page expected
    QTest::addColumn<QStringList>("captions");   // of the pages made, "%1" is the first fixture's name

    QTest::newRow("one fixture") << QStringList({ "Chauvet|Intimidator Beam 140SR V2|14 Channel" })
                                 << QSize(2000, 1250) << false << QStringList({ "%1" });
    // the group page, then a page per model named after it
    QTest::newRow("mixed group") << QStringList({ "American DJ|Mega Flat TRI PAK Plus|6 Channel",
                                                  "Chauvet|Intimidator Beam 140SR V2|14 Channel",
                                                  "Chauvet|Intimidator Wash Zoom 350 IRC|29-Ch",
                                                  "Robe|Robin T1 Profile|3" }) << QSize(2000, 1250) << true
                                 << QStringList({ "4 fixtures", "Mega Flat TRI PAK Plus", "Intimidator Beam 140SR V2",
                                                  "Intimidator Wash Zoom 350 IRC", "Robin T1 Profile" });
    // a section that does not fit is discarded and built on the next page
    QTest::newRow("profile, small window") << QStringList({ "Robe|Robin T1 Profile|3" }) << QSize(1300, 800) << true
                                           << QStringList({ "%1", "%1 (2)", "%1 (3)" });
}

void PagePlanner_Test::consolePageAction()
{
    QFETCH(QStringList, fixtures);
    QFETCH(QSize, window);
    QFETCH(bool, several);
    QFETCH(QStringList, captions);
    const QSize windowBefore = m_app->size();
    m_app->resize(window);
    QTRY_COMPARE(m_app->size(), window);
    const auto restoreWindow = qScopeGuard([this, windowBefore]() { m_app->resize(windowBefore); });
    QList<quint32> ids;
    for (const QString &spec : fixtures)
    {
        const QStringList parts = spec.split('|');
        Fixture *fixture = addFixture(parts.at(0), parts.at(1), parts.at(2));
        QVERIFY(fixture);
        ids.append(fixture->id());
    }

    // the toolbar button of the fixture tree, with the fixtures selected
    auto *contexts = qobject_cast<ContextManager *>(
        m_app->rootContext()->contextProperty("contextManager").value<QObject *>());
    // the main view's own switch loads the Fixtures & Functions view
    QVERIFY(QMetaObject::invokeMethod(m_app->rootObject(), "switchToContext",
                                      Q_ARG(QVariant, QString("FIXANDFUNC")), Q_ARG(QVariant, QString())));
    // open the fixture tree of the left panel, as its "Fixture Groups" button does
    QObject *groups = nullptr;
    QTRY_VERIFY((groups = [&]() -> QObject * {
        for (QObject *object : m_app->rootObject()->findChildren<QObject *>())
            if (object->property("imgSource").toString() == "qrc:/group.svg" && object->property("visible").toBool())
                return object;
        return nullptr;
    }()));
    QObject *button = m_app->rootObject()->findChild<QObject *>("consolePageButton");
    if (button == nullptr)
        QVERIFY(QMetaObject::invokeMethod(groups, "clicked"));
    QTRY_VERIFY((button = m_app->rootObject()->findChild<QObject *>("consolePageButton")));
    contexts->resetFixtureSelection();
    for (quint32 id : ids)
        contexts->setFixtureIDSelection(id, true);
    QTRY_VERIFY(button->property("enabled").toBool());

    VirtualConsole *vc = m_app->virtualConsole();
    const int before = vc->pagesCount();
    const int functionsBefore = m_app->doc()->functions().size();
    settle();
    QVERIFY(QMetaObject::invokeMethod(button, "clicked"));
    const int functionsCreated = m_app->doc()->functions().size() - functionsBefore;
    const int created = vc->pagesCount() - before;
    QVERIFY(created >= 1);
    QCOMPARE(created > 1, several);
    QCOMPARE(vc->selectedPage(), before);

    // every page holds widgets inside the console area of the window
    const QRectF area = m_app->m_consolePageCreator->consoleArea();
    QList<int> widgets;
    for (int p = before; p < vc->pagesCount(); ++p)
    {
        QRectF bounds;
        const QList<VCWidget *> children = vc->page(p)->children(false);
        QVERIFY(!children.isEmpty());
        for (VCWidget *child : children)
            bounds |= child->logicalGeometry();
        QVERIFY2(area.adjusted(-0.5, -0.5, 0.5, 0.5).contains(bounds),
                 qPrintable(QString("page %1: %2x%3 outside %4x%5").arg(p).arg(bounds.right()).arg(bounds.bottom())
                            .arg(area.right()).arg(area.bottom())));
        widgets.append(vc->page(p)->children(true).size());
    }
    QVERIFY(QMetaObject::invokeMethod(m_app->rootObject(), "switchToContext",
                                      Q_ARG(QVariant, QString("VC")), Q_ARG(QVariant, QString("qrc:/VirtualConsole.qml"))));
    for (int p = before; p < vc->pagesCount(); ++p)
        shoot(QString("%1-%2").arg(QString::fromLatin1(QTest::currentDataTag())).arg(p - before + 1), p);
    QStringList made;
    for (int p = before; p < vc->pagesCount(); ++p)
        made << vc->page(p)->caption();
    for (QString &caption : captions)
        caption.replace("%1", m_app->doc()->fixture(ids.first())->name());
    QCOMPARE(made, captions);

    // one undo step takes them away, redo brings them back whole
    settle();
    Tardis::instance()->undoAction();
    settle();
    QCOMPARE(vc->pagesCount(), before);
    QCOMPARE(m_app->doc()->functions().size(), functionsBefore);
    Tardis::instance()->redoAction();
    settle();
    QCOMPARE(vc->pagesCount(), before + created);
    QCOMPARE(m_app->doc()->functions().size(), functionsBefore + functionsCreated);
    for (int p = before; p < vc->pagesCount(); ++p)
    {
        QCOMPARE(vc->page(p)->children(true).size(), widgets.at(p - before));
        // the restored widgets run the restored functions
        QList<VCButton *> buttons;
        QList<VCAnimation *> animations;
        collectWidgets(vc->page(p), buttons);
        collectWidgets(vc->page(p), animations);
        for (VCButton *button : buttons)
            QVERIFY2(m_app->doc()->function(button->functionID()), qPrintable(button->caption()));
        for (VCAnimation *animation : animations)
            QVERIFY(m_app->doc()->function(animation->functionID()));
    }
}

void PagePlanner_Test::functionsRun_data()
{
    QTest::addColumn<QString>("manufacturer");
    QTest::addColumn<QString>("model");
    QTest::addColumn<QString>("mode");
    QTest::addColumn<QString>("widget");    // a button caption, "flash" or "animation"
    QTest::addColumn<QString>("channel");
    QTest::addColumn<bool>("follows");      // a fader beside the buttons shows the pressed value

    QTest::newRow("beam open") << "Chauvet" << "Intimidator Beam 140SR V2" << "14 Channel" << "Open" << "Shutter" << true;
    QTest::newRow("beam prism") << "Chauvet" << "Intimidator Beam 140SR V2" << "14 Channel" << "7-facet prism" << "Prism" << true;
    // the swatch lights up; the picker beside the swatches does not follow
    QTest::newRow("beam red") << "Chauvet" << "Intimidator Beam 140SR V2" << "14 Channel" << "Red" << "Color Wheel" << false;
    QTest::newRow("strobe flash") << "Chauvet" << "DMX Mega Strobe III" << "Standard" << "flash" << "Dimmer" << false;
    QTest::newRow("pixel matrix") << "Blizzard Lighting" << "Pixellicious" << "480 Channel" << "animation" << "" << false;
}

void PagePlanner_Test::functionsRun()
{
    QFETCH(QString, manufacturer);
    QFETCH(QString, model);
    QFETCH(QString, mode);
    QFETCH(QString, widget);
    QFETCH(QString, channel);
    QFETCH(bool, follows);
    Fixture *fixture = addFixture(manufacturer, model, mode);
    QVERIFY(fixture);
    const QList<PagePlanner::Page> pages = PagePlanner::plan(m_app->doc(), { fixture->id() }, fixture->name(), {});
    QList<VCWidget *> roots;
    int used = 0;
    const QString name = QString("run-%1").arg(QString::fromLatin1(QTest::currentDataTag()));
    buildPaged(name, pages.first(), 0, roots, used);
    if (QTest::currentTestFailed())
        return;

    MasterTimer *timer = m_app->doc()->masterTimer();
    timer->start();
    const auto stopTimer = qScopeGuard([timer]() { timer->stop(); });
    const auto output = [&]() {
        QList<Universe *> universes = m_app->doc()->inputOutputMap()->claimUniverses();
        const QByteArray values = universes.at(int(fixture->universe()))->preGMValues();
        m_app->doc()->inputOutputMap()->releaseUniverses(false);
        return values.mid(int(fixture->address()), int(fixture->channels()));
    };
    quint32 index = QLCChannel::invalid();
    for (quint32 i = 0; i < fixture->channels(); ++i)
        if (fixture->channel(i)->name() == channel)
            index = i;

    QList<VCButton *> buttons;
    QList<VCAnimation *> animations;
    QList<VCSlider *> sliders;
    for (VCWidget *root : roots)
    {
        collectWidgets(root, buttons);
        collectWidgets(root, animations);
        collectWidgets(root, sliders);
    }
    // the plain faders on the (not intensity) channel a button sets show what it sends
    const auto followersShowOutput = [&]() {
        int followers = 0;
        for (VCSlider *slider : sliders)
        {
            if (slider->clickAndGoType() != VCSlider::CnGNone ||
                !slider->levelChannels().contains(SceneValue(fixture->id(), index)))
                continue;
            followers++;
            if (slider->value() != int(uchar(output().at(int(index)))))
                return false;
        }
        return followers > 0;
    };

    if (widget == "animation")
    {
        QCOMPARE(animations.size(), 1);
        animations.first()->setFaderLevel(255);
        // the effect lights pixels and moves them
        QTRY_VERIFY(output().count(char(0)) < int(fixture->channels()));
        const QByteArray first = output();
        QTRY_VERIFY(output() != first);
        shoot(name + "-on", 0);
        return;
    }

    QVERIFY(index != QLCChannel::invalid());
    if (widget == "flash")
    {
        VCButton *flash = nullptr;
        for (VCButton *button : buttons)
            if (button->actionType() == VCButton::Flash)
                flash = button;
        QVERIFY(flash);
        flash->requestStateChange(true);
        // a flash still held when the next row resets the workspace crashes it
        auto release = qScopeGuard([flash]() { flash->requestStateChange(false); });
        QTRY_COMPARE(int(uchar(output().at(int(index)))), 255);
        shoot(name + "-on", 0);
        release.dismiss();
        flash->requestStateChange(false);
        QTRY_COMPARE(int(uchar(output().at(int(index)))), 0);
        return;
    }

    // the button of that name whose scene sets the channel (a gobo slot can be "Open" too)
    VCButton *pressed = nullptr;
    for (VCButton *button : buttons)
    {
        const Scene *scene = qobject_cast<const Scene *>(m_app->doc()->function(button->functionID()));
        if (button->caption() == widget && scene && scene->checkValue(SceneValue(fixture->id(), index)))
            pressed = button;
    }
    QVERIFY2(pressed, qPrintable(widget));
    pressed->requestStateChange(true);
    // the channel reaches a value of the capability the button is named after
    const auto inside = [&]() {
        const uchar value = uchar(output().at(int(index)));
        for (const QLCCapability *cap : fixture->channel(index)->capabilities())
            if (cap->name().compare(widget, Qt::CaseInsensitive) == 0 && value >= cap->min() && value <= cap->max())
                return true;
        return false;
    };
    QTRY_VERIFY2(inside(), qPrintable(QString("%1 is %2").arg(channel).arg(uchar(output().at(int(index))))));
    shoot(name + "-on", 0);
    if (follows)
        QTRY_VERIFY(followersShowOutput());
}

QTEST_MAIN(PagePlanner_Test)
