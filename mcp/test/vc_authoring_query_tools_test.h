/*
  Q Light Controller Plus - Unit test
*/

#ifndef VC_AUTHORING_QUERY_TOOLS_TEST_H
#define VC_AUTHORING_QUERY_TOOLS_TEST_H

#include <QObject>

class VCAuthoringQueryTools_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void toolSurface_discoveryAndMetadata_data();
    void toolSurface_discoveryAndMetadata();
    void toolSurface_setupSchemaContract();
    void toolSurface_recordingsSchemaContract();

    void vcQuerySetup_parsesTypedQuery_data();
    void vcQuerySetup_parsesTypedQuery();
    void vcQuerySetup_envelopeAndCursorBehavior();
    void vcQuerySetup_rejectsInvalidArgs_data();
    void vcQuerySetup_rejectsInvalidArgs();

    void vcQueryRecordings_parsesTypedQuery_data();
    void vcQueryRecordings_parsesTypedQuery();
    void vcQueryRecordings_rejectsInvalidArgs_data();
    void vcQueryRecordings_rejectsInvalidArgs();

    void transport_toolResultEncoding_data();
    void transport_toolResultEncoding();

private:
    class Doc *m_doc = nullptr;
};

#endif // VC_AUTHORING_QUERY_TOOLS_TEST_H
