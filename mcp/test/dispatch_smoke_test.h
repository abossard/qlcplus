/*
  Q Light Controller Plus - Unit test
  dispatch_smoke_test.h

  Smoke tests that exercise real ToolManager dispatch end-to-end for
  representative tool families (query, function, palette, channel, IO).
  Verifies tool registration wiring, top-level error shape, and Doc
  side-effects without depending on VC infrastructure.
*/

#ifndef DISPATCH_SMOKE_TEST_H
#define DISPATCH_SMOKE_TEST_H

#include <QObject>

class Doc;

class DispatchSmoke_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void dispatchSmoke_queryFixtures_emptyDoc_returnsArray();
    void dispatchSmoke_createScenes_validItem_createsInDoc();
    void dispatchSmoke_createPalettes_validItem_exists();
    void dispatchSmoke_configureChannels_emptyDoc_returnsArray();
    void dispatchSmoke_configureUniverses_validItem_returnsResult();
    void dispatchSmoke_unknownField_returnsError();
    void dispatchSmoke_setupToolsTouchingLiveOutput_registered();
    void dispatchSmoke_setupAndDiagnosticsTools_remainRegistered();
    void dispatchSmoke_deleteTools_registered();
    void dispatchSmoke_setupAndConfigTools_registered();
    void dispatchSmoke_workspaceTools_needBridge();
    void dispatchSmoke_deleteTools_emptyDoc_returnArrays();

private:
    Doc *m_doc = nullptr;
};

#endif // DISPATCH_SMOKE_TEST_H
