#ifndef VC_TOOL_SURFACE_TEST_H
#define VC_TOOL_SURFACE_TEST_H

#include <QObject>

class Doc;

class VCToolSurface_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();
    void runtimeFields_absentFromSchemas_data();
    void runtimeFields_absentFromSchemas();
    void legacyRuntimeFields_rejectedWithoutActuation_data();
    void legacyRuntimeFields_rejectedWithoutActuation();
    void childPageIndex_createAndUpsert_preservesCurrentPage();
    void invalidChildPageIndex_rejectedBeforeCreateOrUpsert_data();
    void invalidChildPageIndex_rejectedBeforeCreateOrUpsert();
    void setupVCTools_remainRegistered();

private:
    Doc *m_doc = nullptr;
};

#endif // VC_TOOL_SURFACE_TEST_H
