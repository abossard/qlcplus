#ifndef UPSTREAM3D_TEST_H
#define UPSTREAM3D_TEST_H

#include <QObject>

class App;
class ContextManager;
class MainView3D;

class Upstream3D_Test : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void positionDeltas_data();
    void positionDeltas();
    void positionCaller_data();
    void positionCaller();
    void rotationDeltas_data();
    void rotationDeltas();
    void renderQuality_data();
    void renderQuality();
    void multiHeadOffsets_data();
    void multiHeadOffsets();
    void barTilt_data();
    void barTilt();
    void meshCellNotReady_data();
    void meshCellNotReady();
    void rockvilleModes_data();
    void rockvilleModes();
    void stageWizardPointOfView_data();
    void stageWizardPointOfView();

private:
    App *m_app = nullptr;
    ContextManager *m_context = nullptr;
    MainView3D *m_view = nullptr;
};

#endif
