/*
  Q Light Controller Plus
  functionwizardmanager.h

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

#ifndef FUNCTIONWIZARDMANAGER_H
#define FUNCTIONWIZARDMANAGER_H

#include <QQuickView>
#include <QQmlContext>
#include <QObject>
#include <QVariant>

class Doc;
class Fixture;
class VirtualConsole;
class PaletteGenerator;
class VCFrame;

class FunctionWizardManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QVariantList fixtureList READ fixtureList NOTIFY fixtureListChanged)
    Q_PROPERTY(QVariantList availableFixtures READ availableFixtures NOTIFY availableFixturesChanged)
    Q_PROPERTY(QVariantList capabilitiesList READ capabilitiesList NOTIFY capabilitiesListChanged)
    Q_PROPERTY(bool createDedicatedPage READ createDedicatedPage WRITE setCreateDedicatedPage NOTIFY createDedicatedPageChanged)
    Q_PROPERTY(int widgetsPerLine READ widgetsPerLine WRITE setWidgetsPerLine NOTIFY widgetsPerLineChanged)
    Q_PROPERTY(int sliderWidth READ sliderWidth WRITE setSliderWidth NOTIFY sliderWidthChanged)
    Q_PROPERTY(int sliderHeight READ sliderHeight WRITE setSliderHeight NOTIFY sliderHeightChanged)

public:
    FunctionWizardManager(QQuickView *view, Doc *doc, VirtualConsole *vc, QObject *parent = nullptr);
    ~FunctionWizardManager();

    /** Fixture selection */
    Q_INVOKABLE void addFixtures(QVariantList fixtureIDs);
    Q_INVOKABLE void removeFixture(quint32 fixtureID);
    Q_INVOKABLE void clearFixtures();
    Q_INVOKABLE void addAllFixtures();
    QVariantList fixtureList() const;

    /** Available patched fixtures in the project */
    QVariantList availableFixtures() const;

    /** Capabilities - auto-detected from selected fixtures */
    QVariantList capabilitiesList() const;
    Q_INVOKABLE void setCapabilityEnabled(int index, bool enabled);

    /** Widget options */
    bool createDedicatedPage() const;
    void setCreateDedicatedPage(bool create);

    int widgetsPerLine() const;
    void setWidgetsPerLine(int count);

    int sliderWidth() const;
    void setSliderWidth(int width);

    int sliderHeight() const;
    void setSliderHeight(int height);

    /** Execute the wizard - creates functions and VC widgets */
    Q_INVOKABLE bool execute();

    /** Reset wizard state */
    Q_INVOKABLE void reset();

signals:
    void fixtureListChanged();
    void availableFixturesChanged();
    void capabilitiesListChanged();
    void createDedicatedPageChanged();
    void widgetsPerLineChanged();
    void sliderWidthChanged();
    void sliderHeightChanged();

private:
    struct CapabilityEntry {
        QString name;
        int paletteType;
        bool enabled;
    };

    void updateCapabilities();
    void createVCWidgets(QList<PaletteGenerator *> &palettes);
    VCFrame *getTargetFrame();

    QQuickView *m_view;
    Doc *m_doc;
    VirtualConsole *m_vc;

    QList<Fixture *> m_fixtures;
    QList<CapabilityEntry> m_capabilities;

    bool m_createDedicatedPage;
    int m_widgetsPerLine;
    int m_sliderWidth;
    int m_sliderHeight;
};

#endif // FUNCTIONWIZARDMANAGER_H
