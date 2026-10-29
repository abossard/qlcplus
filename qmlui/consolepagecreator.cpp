/*
  Q Light Controller Plus
  consolepagecreator.cpp

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

#include <QQuickView>
#include <QSet>

#include "consolepagecreator.h"
#include "virtualconsole.h"
#include "pagebuilder.h"
#include "fixture.h"
#include "vcpage.h"
#include "tardis.h"
#include "doc.h"

ConsolePageCreator::ConsolePageCreator(Doc *doc, VirtualConsole *vc, QQuickView *view, QObject *parent)
    : QObject(parent)
    , m_doc(doc)
    , m_vc(vc)
    , m_view(view)
{
}

QRectF ConsolePageCreator::consoleArea() const
{
    // the window without the main toolbar and the page tabs above the page,
    // and the side panel handle on its right
    return QRectF(8, 8, m_view->width() - 66, m_view->height() - 126);
}

QList<int> ConsolePageCreator::buildPages(const QList<PagePlanner::Page> &pages, const QRectF &area)
{
    QList<int> created;
    for (const PagePlanner::Page &page : pages)
    {
        PagePlanner::Page rest = page;
        for (int part = 1; !rest.sections.isEmpty(); ++part)
        {
            const int index = m_vc->pagesCount();
            m_vc->addPage(index);
            VCPage *vcPage = m_vc->page(index);
            vcPage->setCaption(part == 1 ? page.title : QString("%1 (%2)").arg(page.title).arg(part));
            created.append(index);
            const PageBuilder::Result result = PageBuilder::build(m_vc, m_doc, vcPage, rest, area);
            if (result.sections == 0)
                break;
            rest.sections = rest.sections.mid(result.sections);
        }
    }
    return created;
}

int ConsolePageCreator::createPages(const QVariantList &fixtureIDs, const QString &title)
{
    QList<quint32> ids;
    QSet<const QLCFixtureMode *> models;
    for (const QVariant &value : fixtureIDs)
    {
        const Fixture *fixture = m_doc->fixture(value.toUInt());
        if (fixture == nullptr || fixture->fixtureMode() == nullptr || ids.contains(fixture->id()))
            continue;
        ids.append(fixture->id());
        models.insert(fixture->fixtureMode());
    }
    if (ids.isEmpty())
        return -1;

    QString name = title;
    if (name.isEmpty())
        name = ids.size() == 1 ? m_doc->fixture(ids.first())->name() : tr("%1 fixtures").arg(ids.size());

    const QRectF area = consoleArea();
    Tardis *tardis = Tardis::instance();
    if (tardis)
        tardis->beginBatch("consolePages");

    const PageBuilder::Made before = PageBuilder::made(m_doc);
    PagePlanner::Options options;
    QList<int> created = buildPages(PagePlanner::plan(m_doc, ids, name, options), area);
    // several models that need more than a page: what they share, then a page per model
    if (created.size() > 1 && models.size() > 1)
    {
        for (int i = created.size() - 1; i >= 0; --i)
            m_vc->deletePage(created.at(i));
        PageBuilder::deleteMadeSince(m_doc, before);
        options.strategy = PagePlanner::MasterAndModels;
        created = buildPages(PagePlanner::plan(m_doc, ids, name, options), area);
    }

    if (tardis)
        tardis->endBatch();
    return created.isEmpty() ? -1 : created.first();
}
