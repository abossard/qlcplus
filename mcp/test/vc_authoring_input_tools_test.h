/*
  Q Light Controller Plus - Unit test
  vc_authoring_input_tools_test.h
*/

#ifndef VC_AUTHORING_INPUT_TOOLS_TEST_H
#define VC_AUTHORING_INPUT_TOOLS_TEST_H

#include <QObject>

class Doc;

class VCAuthoringInputTools_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void tools_registeredWithExpectedSchemasAndAnnotations_data();
    void tools_registeredWithExpectedSchemasAndAnnotations();
    void tools_schemaUsesKMaxIdForIdBounds_data();
    void tools_schemaUsesKMaxIdForIdBounds();

    void queryControls_capturesTypedQuery_data();
    void queryControls_capturesTypedQuery();
    void queryControls_rejectsInvalidRequestsBeforeBridge_data();
    void queryControls_rejectsInvalidRequestsBeforeBridge();

    void prepareBindings_capturesTypedRequest_data();
    void prepareBindings_capturesTypedRequest();
    void prepareBindings_rejectsInvalidRequestsBeforeBridge_data();
    void prepareBindings_rejectsInvalidRequestsBeforeBridge();
    void prepareBindings_rejectsItemCountBounds();

    void authoringInputTools_bridgeResultShapeConsistency();

private:
    Doc *m_doc = nullptr;
};

#endif // VC_AUTHORING_INPUT_TOOLS_TEST_H
