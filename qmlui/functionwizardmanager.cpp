/*
  Q Light Controller Plus
  functionwizardmanager.cpp

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

#include <QQmlEngine>
#include <QQmlContext>
#include <QDebug>

#include "functionwizardmanager.h"
#include "palettegenerator.h"
#include "virtualconsole.h"
#include "vcframe.h"
#include "vcpage.h"
#include "vcbutton.h"
#include "vcslider.h"
#include "vcxypad.h"
#include "vccuelist.h"
#include "vcsoloframe.h"
#include "qlcchannel.h"
#include "fixture.h"
#include "scene.h"
#include "chaser.h"
#include "rgbmatrix.h"
#include "doc.h"

#include "tardis.h"

FunctionWizardManager::FunctionWizardManager(QQuickView *view, Doc *doc,
                                             VirtualConsole *vc, QObject *parent)
    : QObject(parent)
    , m_view(view)
    , m_doc(doc)
    , m_vc(vc)
    , m_createDedicatedPage(true)
    , m_perFixturePage(true)
    , m_beatSync(true)
    , m_widgetsPerLine(8)
    , m_sliderWidth(60)
    , m_sliderHeight(200)
{
    Q_ASSERT(doc != nullptr);
    Q_ASSERT(vc != nullptr);

    m_view->rootContext()->setContextProperty("functionWizardManager", this);
}

FunctionWizardManager::~FunctionWizardManager()
{
    m_view->rootContext()->setContextProperty("functionWizardManager", nullptr);
}

/********************************************************************
 * Fixture selection
 ********************************************************************/

void FunctionWizardManager::addFixtures(QVariantList fixtureIDs)
{
    for (const QVariant &v : fixtureIDs)
    {
        quint32 fxID = v.toUInt();
        Fixture *fxi = m_doc->fixture(fxID);
        if (fxi == nullptr)
            continue;

        // avoid duplicates
        bool found = false;
        for (Fixture *existing : m_fixtures)
        {
            if (existing->id() == fxID)
            {
                found = true;
                break;
            }
        }
        if (!found)
            m_fixtures.append(fxi);
    }

    updateCapabilities();
    emit fixtureListChanged();
    emit availableFixturesChanged();
}

void FunctionWizardManager::removeFixture(quint32 fixtureID)
{
    for (int i = 0; i < m_fixtures.count(); i++)
    {
        if (m_fixtures.at(i)->id() == fixtureID)
        {
            m_fixtures.removeAt(i);
            break;
        }
    }

    updateCapabilities();
    emit fixtureListChanged();
    emit availableFixturesChanged();
}

void FunctionWizardManager::clearFixtures()
{
    m_fixtures.clear();
    m_capabilities.clear();
    emit fixtureListChanged();
    emit availableFixturesChanged();
    emit capabilitiesListChanged();
}

QVariantList FunctionWizardManager::fixtureList() const
{
    QVariantList list;
    for (Fixture *fxi : m_fixtures)
    {
        QVariantMap entry;
        entry["id"] = fxi->id();
        entry["name"] = fxi->name();
        entry["manufacturer"] = fxi->fixtureDef() ? fxi->fixtureDef()->manufacturer() : tr("Generic");
        entry["model"] = fxi->fixtureDef() ? fxi->fixtureDef()->model() : tr("Generic");
        list.append(entry);
    }
    return list;
}

QVariantList FunctionWizardManager::availableFixtures() const
{
    QVariantList list;
    for (Fixture *fxi : m_doc->fixtures())
    {
        QVariantMap entry;
        entry["id"] = fxi->id();
        entry["name"] = fxi->name();
        entry["manufacturer"] = fxi->fixtureDef() ? fxi->fixtureDef()->manufacturer() : tr("Generic");
        entry["model"] = fxi->fixtureDef() ? fxi->fixtureDef()->model() : tr("Generic");
        entry["channels"] = fxi->channels();

        // Check if already selected
        bool selected = false;
        for (Fixture *sel : m_fixtures)
        {
            if (sel->id() == fxi->id())
            {
                selected = true;
                break;
            }
        }
        entry["selected"] = selected;
        list.append(entry);
    }
    return list;
}

void FunctionWizardManager::addAllFixtures()
{
    m_fixtures.clear();
    for (Fixture *fxi : m_doc->fixtures())
        m_fixtures.append(fxi);

    updateCapabilities();
    emit fixtureListChanged();
    emit availableFixturesChanged();
}

/********************************************************************
 * Capabilities
 ********************************************************************/

void FunctionWizardManager::updateCapabilities()
{
    m_capabilities.clear();

    if (m_fixtures.isEmpty())
    {
        emit capabilitiesListChanged();
        return;
    }

    // Gather capabilities from the first fixture (all should be same model)
    QStringList caps = PaletteGenerator::getCapabilities(m_fixtures.at(0));

    // Map capability strings to palette types
    if (caps.contains(KQLCChannelRGB) || caps.contains(KQLCChannelCMY))
    {
        m_capabilities.append({tr("Primary Colors"), PaletteGenerator::PrimaryColors, true});
        m_capabilities.append({tr("16 Colors"), PaletteGenerator::SixteenColors, true});
        m_capabilities.append({tr("Rainbow Chase"), PaletteGenerator::RainbowChaser, false});
        m_capabilities.append({tr("Warm Colours"), PaletteGenerator::WarmColors, false});
        m_capabilities.append({tr("Cool Colours"), PaletteGenerator::CoolColors, false});
    }

    if (caps.contains(QLCChannel::groupToString(QLCChannel::Shutter)))
        m_capabilities.append({tr("Shutter Macros"), PaletteGenerator::Shutter, true});

    if (caps.contains(QLCChannel::groupToString(QLCChannel::Gobo)))
        m_capabilities.append({tr("Gobo Macros"), PaletteGenerator::Gobos, true});

    if (caps.contains(QLCChannel::groupToString(QLCChannel::Colour)))
        m_capabilities.append({tr("Colour Macros"), PaletteGenerator::ColourMacro, true});

    if (caps.contains(QLCChannel::groupToString(QLCChannel::Prism)))
        m_capabilities.append({tr("Prism Effects"), PaletteGenerator::Prism, true});

    if (caps.contains(KQLCChannelRGB))
        m_capabilities.append({tr("Animations"), PaletteGenerator::Animation, true});

    if (caps.contains(KQLCChannelMovement))
        m_capabilities.append({tr("Pan/Tilt Positions"), PaletteGenerator::PanTilt, true});

    if (caps.contains(QStringLiteral("Dimmer")))
        m_capabilities.append({tr("Dimmer Levels"), PaletteGenerator::Dimmer, true});

    emit capabilitiesListChanged();
}

QVariantList FunctionWizardManager::capabilitiesList() const
{
    QVariantList list;
    for (int i = 0; i < m_capabilities.count(); i++)
    {
        QVariantMap entry;
        entry["name"] = m_capabilities.at(i).name;
        entry["type"] = m_capabilities.at(i).paletteType;
        entry["enabled"] = m_capabilities.at(i).enabled;
        list.append(entry);
    }
    return list;
}

void FunctionWizardManager::setCapabilityEnabled(int index, bool enabled)
{
    if (index < 0 || index >= m_capabilities.count())
        return;

    m_capabilities[index].enabled = enabled;
    emit capabilitiesListChanged();
}

/********************************************************************
 * Widget options
 ********************************************************************/

bool FunctionWizardManager::createDedicatedPage() const
{
    return m_createDedicatedPage;
}

void FunctionWizardManager::setCreateDedicatedPage(bool create)
{
    if (m_createDedicatedPage == create)
        return;
    m_createDedicatedPage = create;
    emit createDedicatedPageChanged();
}

bool FunctionWizardManager::perFixturePage() const
{
    return m_perFixturePage;
}

void FunctionWizardManager::setPerFixturePage(bool perFixture)
{
    if (m_perFixturePage == perFixture)
        return;
    m_perFixturePage = perFixture;
    emit perFixturePageChanged();
}

bool FunctionWizardManager::beatSync() const
{
    return m_beatSync;
}

void FunctionWizardManager::setBeatSync(bool sync)
{
    if (m_beatSync == sync)
        return;
    m_beatSync = sync;
    emit beatSyncChanged();
}

int FunctionWizardManager::widgetsPerLine() const
{
    return m_widgetsPerLine;
}

void FunctionWizardManager::setWidgetsPerLine(int count)
{
    if (m_widgetsPerLine == count)
        return;
    m_widgetsPerLine = qMax(1, count);
    emit widgetsPerLineChanged();
}

int FunctionWizardManager::sliderWidth() const
{
    return m_sliderWidth;
}

void FunctionWizardManager::setSliderWidth(int width)
{
    if (m_sliderWidth == width)
        return;
    m_sliderWidth = qMax(20, width);
    emit sliderWidthChanged();
}

int FunctionWizardManager::sliderHeight() const
{
    return m_sliderHeight;
}

void FunctionWizardManager::setSliderHeight(int height)
{
    if (m_sliderHeight == height)
        return;
    m_sliderHeight = qMax(40, height);
    emit sliderHeightChanged();
}

/********************************************************************
 * Execute wizard
 ********************************************************************/

VCFrame *FunctionWizardManager::getTargetFrame()
{
    if (m_createDedicatedPage)
    {
        int newPageIdx = m_vc->pagesCount();
        m_vc->addPage(newPageIdx);
        VCPage *page = m_vc->page(newPageIdx);
        if (page)
        {
            m_vc->setSelectedPage(newPageIdx);
            return page;
        }
    }

    // Use current page
    VCPage *page = m_vc->page(m_vc->selectedPage());
    return page;
}

VCFrame *FunctionWizardManager::createFixturePage(const QString &name)
{
    int newPageIdx = m_vc->pagesCount();
    m_vc->addPage(newPageIdx);
    VCPage *page = m_vc->page(newPageIdx);
    if (page)
    {
        page->setCaption(name);
        m_vc->setSelectedPage(newPageIdx);
    }
    return page;
}

void FunctionWizardManager::createFocusZoomSliders(VCFrame *targetFrame, int &xPos, int &yPos)
{
    if (m_fixtures.isEmpty())
        return;

    qreal pd = m_vc->pixelDensity();

    for (Fixture *fxi : m_fixtures)
    {
        for (quint32 ch = 0; ch < fxi->channels(); ch++)
        {
            const QLCChannel *channel = fxi->channel(ch);
            if (channel == nullptr)
                continue;

            if (channel->group() != QLCChannel::Beam)
                continue;

            QLCChannel::Preset preset = channel->preset();
            bool isFocus = (preset == QLCChannel::BeamFocusNearFar ||
                            preset == QLCChannel::BeamFocusFarNear);
            bool isZoom = (preset == QLCChannel::BeamZoomSmallBig ||
                           preset == QLCChannel::BeamZoomBigSmall);

            // Skip fine channels
            if (preset == QLCChannel::BeamFocusFine ||
                preset == QLCChannel::BeamZoomFine)
                continue;

            if (!isFocus && !isZoom)
                continue;

            VCSlider *slider = new VCSlider(m_doc, targetFrame);
            slider->setCaption(isFocus ? tr("Focus") : tr("Zoom"));
            slider->setSliderMode(VCSlider::Level);
            slider->addLevelChannel(fxi->id(), ch);

            int sw = static_cast<int>(pd * 15);
            int sh = static_cast<int>(pd * 50);
            slider->setGeometry(QRect(xPos, yPos, sw, sh));
            targetFrame->addWidget(nullptr, slider, QPoint(xPos, yPos));

            xPos += sw + 10;
        }
    }
}

bool FunctionWizardManager::execute()
{
    if (m_fixtures.isEmpty())
        return false;

    bool anyEnabled = false;
    for (const CapabilityEntry &cap : m_capabilities)
    {
        if (cap.enabled)
        {
            anyEnabled = true;
            break;
        }
    }
    if (!anyEnabled)
        return false;

    if (m_perFixturePage && m_fixtures.count() > 1)
    {
        // Per-fixture mode: each fixture gets its own page
        QList<Fixture *> allFixtures = m_fixtures;

        for (Fixture *fxi : allFixtures)
        {
            m_fixtures.clear();
            m_fixtures.append(fxi);

            QList<PaletteGenerator *> palettes;
            for (const CapabilityEntry &cap : m_capabilities)
            {
                if (!cap.enabled)
                    continue;

                PaletteGenerator *pg = new PaletteGenerator(
                    m_doc, m_fixtures,
                    static_cast<PaletteGenerator::PaletteType>(cap.paletteType),
                    PaletteGenerator::All);

                if (pg->scenes().count() > 0 || pg->chasers().count() > 0 ||
                    pg->matrices().count() > 0 || pg->palettes().count() > 0)
                {
                    pg->addToDoc();
                    palettes.append(pg);
                }
                else
                {
                    delete pg;
                }
            }

            if (!palettes.isEmpty())
            {
                // Force dedicated page for this fixture
                bool savedDedicatedPage = m_createDedicatedPage;
                m_createDedicatedPage = true;
                createVCWidgets(palettes);
                m_createDedicatedPage = savedDedicatedPage;

                // Set page caption to fixture name
                VCPage *page = m_vc->page(m_vc->selectedPage());
                if (page)
                    page->setCaption(fxi->name());
            }

            qDeleteAll(palettes);
        }

        // Restore the full fixture list
        m_fixtures = allFixtures;
    }
    else
    {
        // Original behavior: all fixtures together
        QList<PaletteGenerator *> palettes;
        for (const CapabilityEntry &cap : m_capabilities)
        {
            if (!cap.enabled)
                continue;

            PaletteGenerator *pg = new PaletteGenerator(
                m_doc, m_fixtures,
                static_cast<PaletteGenerator::PaletteType>(cap.paletteType),
                PaletteGenerator::All);

            if (pg->scenes().count() > 0 || pg->chasers().count() > 0 ||
                pg->matrices().count() > 0 || pg->palettes().count() > 0)
            {
                pg->addToDoc();
                palettes.append(pg);
            }
            else
            {
                delete pg;
            }
        }

        if (!palettes.isEmpty())
            createVCWidgets(palettes);

        qDeleteAll(palettes);
    }

    m_doc->setModified();
    return true;
}

void FunctionWizardManager::createVCWidgets(QList<PaletteGenerator *> &palettes)
{
    VCFrame *targetFrame = getTargetFrame();
    if (targetFrame == nullptr)
        return;

    int xPos = 10;
    int yPos = 10;
    qreal pd = m_vc->pixelDensity();

    for (PaletteGenerator *palette : palettes)
    {
        // Create a labeled frame for this palette group
        VCFrame *groupFrame = new VCFrame(m_doc, m_vc, targetFrame);
        groupFrame->setCaption(palette->fullName());

        int frameWidth = 0;
        int frameHeight = 0;
        int innerX = 5;
        int innerY = 30; // leave room for frame header
        int colCount = 0;

        // Determine if scene buttons should be wrapped in a SoloFrame
        bool useSoloForScenes = (palette->type() == PaletteGenerator::PrimaryColors ||
                                 palette->type() == PaletteGenerator::SixteenColors ||
                                 palette->type() == PaletteGenerator::RainbowChaser ||
                                 palette->type() == PaletteGenerator::WarmColors ||
                                 palette->type() == PaletteGenerator::CoolColors ||
                                 palette->type() == PaletteGenerator::Gobos ||
                                 palette->type() == PaletteGenerator::ColourMacro ||
                                 palette->type() == PaletteGenerator::Prism);

        // Create buttons for scenes (optionally in a SoloFrame)
        if (palette->scenes().count() > 0)
        {
            VCFrame *buttonContainer = groupFrame;
            int containerInnerX = innerX;
            int containerInnerY = innerY;

            if (useSoloForScenes)
            {
                VCSoloFrame *soloFrame = new VCSoloFrame(m_doc, m_vc, groupFrame);
                soloFrame->setCaption(palette->fullName());
                buttonContainer = soloFrame;
                containerInnerX = 5;
                containerInnerY = 30;
            }

            int sfWidth = 0;
            int sfHeight = 0;
            int sfColCount = 0;

            for (Scene *scene : palette->scenes())
            {
                VCButton *button = new VCButton(m_doc, buttonContainer);
                button->setFunctionID(scene->id());
                button->setCaption(scene->name());
                int bw = static_cast<int>(pd * 17);
                int bh = static_cast<int>(pd * 17);
                button->setGeometry(QRect(containerInnerX, containerInnerY, bw, bh));
                buttonContainer->addWidget(nullptr, button, QPoint(containerInnerX, containerInnerY));

                containerInnerX += bw + 5;
                sfColCount++;
                if (sfColCount >= m_widgetsPerLine)
                {
                    sfColCount = 0;
                    containerInnerX = 5;
                    containerInnerY += bh + 5;
                }

                if (containerInnerX + bw > sfWidth)
                    sfWidth = containerInnerX + bw;
                if (containerInnerY + bh > sfHeight)
                    sfHeight = containerInnerY + bh;
            }

            if (useSoloForScenes)
            {
                VCSoloFrame *soloFrame = qobject_cast<VCSoloFrame *>(buttonContainer);
                sfWidth = qMax(sfWidth + 5, 200);
                sfHeight = qMax(sfHeight + 5, 60);
                soloFrame->setGeometry(QRect(innerX, innerY, sfWidth, sfHeight));
                groupFrame->addWidget(nullptr, soloFrame, QPoint(innerX, innerY));

                frameWidth = qMax(frameWidth, innerX + sfWidth);
                frameHeight = innerY + sfHeight;
                innerY = frameHeight + 10;
                innerX = 5;
                colCount = 0;
            }
            else
            {
                frameWidth = qMax(frameWidth, sfWidth);
                frameHeight = sfHeight;
                innerX = containerInnerX;
                innerY = containerInnerY;
                colCount = sfColCount;
            }
        }

        // Create cue lists for chasers
        if (palette->chasers().count() > 0)
        {
            if (colCount > 0)
            {
                innerX = 5;
                innerY = frameHeight + 10;
                colCount = 0;
            }

            for (Chaser *chaser : palette->chasers())
            {
                VCCueList *cuelist = new VCCueList(m_doc, groupFrame);
                cuelist->setChaserID(chaser->id());
                cuelist->setCaption(chaser->name());
                int cw = static_cast<int>(pd * 80);
                int ch = static_cast<int>(pd * 40);
                cuelist->setGeometry(QRect(innerX, innerY, cw, ch));
                groupFrame->addWidget(nullptr, cuelist, QPoint(innerX, innerY));

                innerX += cw + 5;

                if (innerX > frameWidth)
                    frameWidth = innerX;
                if (innerY + ch > frameHeight)
                    frameHeight = innerY + ch;
            }
        }

        // Create buttons for RGB matrices in a solo frame
        if (palette->matrices().count() > 0)
        {
            if (colCount > 0 || !palette->chasers().isEmpty())
            {
                innerX = 5;
                innerY = frameHeight + 10;
                colCount = 0;
            }

            VCSoloFrame *soloFrame = new VCSoloFrame(m_doc, m_vc, groupFrame);
            soloFrame->setCaption(tr("Animations"));

            int sfInnerX = 5;
            int sfInnerY = 30;
            int sfColCount = 0;

            for (RGBMatrix *matrix : palette->matrices())
            {
                VCButton *button = new VCButton(m_doc, soloFrame);
                button->setFunctionID(matrix->id());
                button->setCaption(matrix->name());
                int bw = static_cast<int>(pd * 17);
                int bh = static_cast<int>(pd * 17);
                button->setGeometry(QRect(sfInnerX, sfInnerY, bw, bh));
                soloFrame->addWidget(nullptr, button, QPoint(sfInnerX, sfInnerY));

                sfInnerX += bw + 5;
                sfColCount++;
                if (sfColCount >= m_widgetsPerLine)
                {
                    sfColCount = 0;
                    sfInnerX = 5;
                    sfInnerY += bh + 5;
                }
            }

            int sfWidth = qMin(static_cast<int>(pd * 17 + 5) * qMin(m_widgetsPerLine, palette->matrices().count()) + 5,
                               frameWidth > 0 ? frameWidth : 9999);
            int sfHeight = sfInnerY + static_cast<int>(pd * 17) + 5;
            soloFrame->setGeometry(QRect(innerX, innerY, sfWidth, sfHeight));
            groupFrame->addWidget(nullptr, soloFrame, QPoint(innerX, innerY));

            if (innerX + sfWidth > frameWidth)
                frameWidth = innerX + sfWidth;
            frameHeight = innerY + sfHeight;
        }

        // Create XY Pad for Pan/Tilt palettes
        if (palette->type() == PaletteGenerator::PanTilt)
        {
            innerX = 5;
            innerY = frameHeight + 10;

            VCXYPad *xypad = new VCXYPad(m_doc, groupFrame);
            xypad->setCaption(tr("Pan/Tilt Pad"));
            for (Fixture *fx : m_fixtures)
            {
                for (int h = 0; h < fx->heads(); h++)
                    xypad->addHead(fx->id(), h);
            }

            int padSize = static_cast<int>(pd * 50);
            xypad->setGeometry(QRect(innerX, innerY, padSize, padSize));
            groupFrame->addWidget(nullptr, xypad, QPoint(innerX, innerY));

            if (innerX + padSize > frameWidth)
                frameWidth = innerX + padSize;
            frameHeight = innerY + padSize;
        }

        // Create Slider for Dimmer palettes
        if (palette->type() == PaletteGenerator::Dimmer)
        {
            innerX = 5;
            innerY = frameHeight + 10;

            VCSlider *slider = new VCSlider(m_doc, groupFrame);
            slider->setCaption(tr("Dimmer"));
            slider->setSliderMode(VCSlider::Level);
            for (Fixture *fx : m_fixtures)
            {
                for (quint32 ch = 0; ch < fx->channels(); ch++)
                {
                    const QLCChannel *channel = fx->channel(ch);
                    if (channel->group() == QLCChannel::Intensity &&
                        channel->colour() == QLCChannel::NoColour)
                    {
                        slider->addLevelChannel(fx->id(), ch);
                    }
                }
            }

            int sw = static_cast<int>(pd * 15);
            int sh = static_cast<int>(pd * 50);
            slider->setGeometry(QRect(innerX, innerY, sw, sh));
            groupFrame->addWidget(nullptr, slider, QPoint(innerX, innerY));

            if (innerX + sw > frameWidth)
                frameWidth = innerX + sw;
            frameHeight = innerY + sh;
        }

        // Size and position the group frame
        frameWidth = qMax(frameWidth + 10, 200);
        frameHeight = qMax(frameHeight + 10, 100);
        groupFrame->setGeometry(QRect(xPos, yPos, frameWidth, frameHeight));
        targetFrame->addWidget(nullptr, groupFrame, QPoint(xPos, yPos));

        // Advance position for next group
        yPos += frameHeight + 10;
    }

    // Add Focus/Zoom sliders at the end
    createFocusZoomSliders(targetFrame, xPos, yPos);

    // Grow the page height if widgets exceed the default bounds
    VCPage *page = qobject_cast<VCPage *>(targetFrame);
    if (page)
        page->adjustPageHeight();
}

void FunctionWizardManager::reset()
{
    m_fixtures.clear();
    m_capabilities.clear();
    m_createDedicatedPage = true;
    m_perFixturePage = true;
    m_beatSync = true;
    m_widgetsPerLine = 8;
    m_sliderWidth = 60;
    m_sliderHeight = 200;

    emit fixtureListChanged();
    emit availableFixturesChanged();
    emit capabilitiesListChanged();
    emit createDedicatedPageChanged();
    emit perFixturePageChanged();
    emit beatSyncChanged();
    emit widgetsPerLineChanged();
    emit sliderWidthChanged();
    emit sliderHeightChanged();
}
