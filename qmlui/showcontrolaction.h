#ifndef SHOWCONTROLACTION_H
#define SHOWCONTROLACTION_H

#include <QPointer>
#include <QPointF>
#include <QVector>
#include <QColor>
#include <QVector3D>
#include <QHash>
#include <QSet>
#include <QVariantMap>
#include <functional>

#include "show.h"

class Doc;
class VCWidget;
class VirtualConsole;
struct ShowControlConfiguration;
struct ShowControlRequest;

class ShowControlAction
{
public:
    struct Values
    {
        ShowControlRole role = ShowControlRole::None;
        int scalar = 0;
        bool overriding = false;
        QPointF point;
        QVector3D floor;
        bool floorMode = false;
        QVector3D floorSize;
        QPointF horizontal, vertical;
        int choice = -1;
        FunctionParent choiceOwner = FunctionParent::master();
        QVariantMap binding;
        QVector<bool> headEnabled;
        QVector<QColor> colors;
        bool nativeColorValue = false;
        bool algorithmOverride = false, contentOverride = false;
        QString algorithm, text;
        QMap<QString, QString> properties;
    };
    struct State : Values
    {
        QPointer<QObject> choiceLifetime;
        QPointer<QObject> choiceFunctionLifetime;
        QPointer<QObject> matrixGroupLifetime;
        QSize matrixGroupSize;
        QVector<QPointer<QObject>> lifetimes;
    };
    struct RestorePlan
    {
        bool scalar = false, overriding = false, point = false, floor = false;
        bool ranges = false, choice = false, colors = false, content = false;
        bool superseded = false;
    };
    struct Receipt
    {
        QPointer<VCWidget> control;
        quint64 writerGeneration = 0;
        QVector<ShowFunctionExpectation> functions;
        QVector<QPair<QPointer<VCWidget>, quint64>> writes;
        ShowCommandFsm::ShowButtonOp buttonOp = ShowCommandFsm::ShowButtonOp::None;
        QString refusal;
        bool requested = false, unchanged = false;
        bool atomicNativeState = false;
        bool executionFailed = false;
        State before, after;
    };
    struct CapturePlan
    {
        ShowCommandInput input;
        bool supported = false;
        bool unchanged = false;
    };
    struct ReceiptPlan
    {
        bool depends = false;
        QVector<ShowFunctionExpectation> functions;
    };
    struct Resolution
    {
        QPointer<VCWidget> control;
        ShowControlStatus status = ShowControlStatus::Missing;
        QString reason;
    };
    struct ClosureFact
    {
        QPointer<VCWidget> control;
        State before, after;
        bool ownerBefore = false;
        bool conditionalOnStart = false;
    };
    struct Restoration
    {
        QPointer<VCWidget> control;
        State before, after;
        FunctionParent owner = FunctionParent::master();
        bool releaseOwner = false;
        bool superseded = false;
    };
    struct Holds
    {
        QSet<quint32> freeze, flash, sliderFlash;
    };
    struct Stop
    {
        FunctionParent owner = FunctionParent::master();
        QVector<Restoration> effects;
        QSet<QUuid> freeze, flash, sliderFlash;
        Holds user;
        std::optional<bool> freezeIntent, blackoutIntent;
    };

    static Resolution resolve(Doc *doc, VirtualConsole *vc, const ShowCommand &command);
    static ShowControlCoupling coupling(Doc *doc, VirtualConsole *vc, VCWidget *control, quint32 functionId);
    static ReceiptPlan planReceipt(const ShowControlCoupling &done,
                                   ShowCommandFsm::ShowButtonOp operation,
                                   const ShowControlCoupling *next = nullptr);
    static QVector<ClosureFact> closure(Doc *doc, VirtualConsole *vc, VCWidget *control,
                                       const FunctionParent &owner);
    static ShowControlConfiguration configuration(Doc *doc, VCWidget *control,
                                                  const ShowCommandInput *input = nullptr);
    static QString readiness(VCWidget *control, const ShowCommand &command);
    static QString preflight(VCWidget *control, const ShowCommandInput &input);
    static QVariantMap editorInfo(VCWidget *control, const QString &algorithm);
    static quint32 functionId(VCWidget *control, const ShowCommandInput *input = nullptr);
    static int scalarState(VCWidget *control);
    static ShowCommandInput acceptedInput(const ShowControlRequest &request);
    static CapturePlan planCapture(const ShowControlRequest &request, const Values &before,
                                   const ShowControlRequest *previous);
    static ShowCommandInput acceptedPreset(VCWidget *control, int choice, int knobValue = -1);
    static bool submit(Doc *doc, VCWidget *control, const ShowCommandInput &input);
    static Receipt apply(Doc *doc, VCWidget *control, const ShowCommandInput &input,
                         const FunctionParent &owner, bool replay, bool strictRelease = false,
                         const ShowControlRequest *live = nullptr);
    static bool release(Doc *doc, VCWidget *control, const FunctionParent &owner,
                        const State *actual = nullptr);
    static void releaseHold(VirtualConsole *vc, VCWidget *control, ShowControlRole role,
                            const FunctionParent &owner, const Holds &user, bool stopping);
    static Holds stop(Doc *doc, VirtualConsole *vc, const Stop &state);
    static QVector<QMetaObject::Connection> observeConfiguration(
        VCWidget *control, QObject *receiver, const std::function<void()> &refresh);
    static void connectWriter(VCWidget *control, QObject *receiver);
    static void cancelWriter(VCWidget *control, quint64 generation);
    static std::optional<ShowFunctionExpectation> writerExpectation(quint32 functionId, int effect);
    static State observe(VCWidget *control);
    static RestorePlan planRestore(const Values &before, const Values &after, const Values &current,
                                   bool sameBindingLifetimes = true, bool sameChoiceLifetime = true);
    static Receipt restore(Doc *doc, VCWidget *control, const State &before,
                           const State &after, const FunctionParent &owner);
    static QVector<Receipt> restoreBatch(Doc *doc, VirtualConsole *vc,
                                        const QVector<Restoration> &targets,
                                        const QHash<VCWidget *, State> &latest,
                                        const QSet<quint32> &flashHolds,
                                        const QSet<quint32> &sliderFlashHolds);
};

#endif
