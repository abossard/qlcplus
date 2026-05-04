/*
  Q Light Controller Plus - Unit test
*/

#ifndef QUERY_TOOLS_TEST_H
#define QUERY_TOOLS_TEST_H

#include <QObject>

class QueryTools_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void queryPalettes_invalidTypeFilterReturnsError();
    void queryRgbAlgorithms_invalidTypeReturnsError();
    void queryWorkspaceSummary_returnsExpectedCounts();
    void queryWorkspaceSummary_populatedDoc_returnsExactCounts();

private:
    class Doc *m_doc = nullptr;
};

#endif // QUERY_TOOLS_TEST_H
