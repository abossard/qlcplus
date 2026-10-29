/*
  Q Light Controller Plus - Unit test
  pageplanner_test.h

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

#ifndef PAGEPLANNER_TEST_H
#define PAGEPLANNER_TEST_H

#include <QObject>
#include <QStringList>
#include <QTemporaryDir>
#include <memory>

#include "pageplanner.h"

class App;
class VCWidget;
class Fixture;

/** What measure() found on the last page */
struct PageResult
{
    int structureErrors = 0;
    int pads = 0;
    int reachAny = 0;
    /** Captions that do not fit their widget */
    QStringList misfitCaptions;
};

/** A named set of fixtures, a group of the rig */
struct RigGroup
{
    QString name;
    QList<quint32> fixtures;
};

class PagePlanner_Test : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanupTestCase();

    void presetRule_data();
    void presetRule();
    void choiceLabels_data();
    void choiceLabels();
    void channelSection_data();
    void channelSection();
    void sectionOrder_data();
    void sectionOrder();
    void sectionOrderTranslated();
    void libraryReach();
    void referencePage_data();
    void referencePage();
    void wizardRig_data();
    void wizardRig();
    void engineRig_data();
    void engineRig();
    void consolePageAction_data();
    void consolePageAction();
    void functionsRun_data();
    void functionsRun();

private:
    Fixture *addFixture(const QString &manufacturer, const QString &model, const QString &mode);
    /** The "main" group rig or the "gaps" rig of known Stage Wizard gaps */
    QList<RigGroup> buildRig(const QString &rig);
    /** Wait until Tardis recorded every queued action, then long enough for
     *  the next action to start a new undo step */
    void settle();
    /** Show VC page $page and keep the window as $name.png */
    void shoot(const QString &name, int page);
    /** Build $page from console page $first on, a console page per part that
     *  fits the console of a 1920x1080 window on a real 1080p screen (pd 4.9),
     *  and shoot each. $roots gets the frames of every part, $used the pages */
    void buildPaged(const QString &name, const PagePlanner::Page &page, int first,
                    QList<VCWidget *> &roots, int &used);
    /** Measure the widgets $roots (only their subpage $subpage, -1 for all)
     *  for the fixtures $fixtures and append a row to metrics.tsv */
    void measure(const QString &name, const QList<quint32> &fixtures, const QList<VCWidget *> &roots,
                 int subpage, int functions);

private:
    QTemporaryDir m_settings;
    std::unique_ptr<App> m_app;
    quint32 m_nextAddress = 0;
    PageResult m_lastPage;
};

#endif
