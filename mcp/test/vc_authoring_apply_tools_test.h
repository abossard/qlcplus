/*
  Q Light Controller Plus - Unit test
  vc_authoring_apply_tools_test.h
*/

#ifndef VC_AUTHORING_APPLY_TOOLS_TEST_H
#define VC_AUTHORING_APPLY_TOOLS_TEST_H

#include <QObject>

class Doc;

class VCAuthoringApplyTools_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void tool_registeredWithExpectedSchemaAndAnnotations();
    void prepareLayout_registeredWithExpectedSchemaAndAnnotations();
    void prepareLayout_rejectsInvalidRequestsBeforeBridge_data();
    void prepareLayout_rejectsInvalidRequestsBeforeBridge();
    void prepareLayout_capturesTypedRequestAndReturnsBridgePayload();
    void prepareLayout_capturesTypedPageOps_data();
    void prepareLayout_capturesTypedPageOps();
    void prepareRecordPanel_registeredWithExpectedSchemaAndAnnotations();
    void prepareRecordPanel_rejectsInvalidRequestsBeforeBridge_data();
    void prepareRecordPanel_rejectsInvalidRequestsBeforeBridge();
    void prepareRecordPanel_capturesTypedRequestAndReturnsBridgePayload();
    void applyPreparation_rejectsInvalidRequestsBeforeBridge_data();
    void applyPreparation_rejectsInvalidRequestsBeforeBridge();
    void applyPreparation_invokesBridgeAndReturnsBridgePayload();
    void updateWidgets_recordPanel_acceptsValidPartialConfig();

private:
    Doc *m_doc = nullptr;
};

#endif // VC_AUTHORING_APPLY_TOOLS_TEST_H
