/*
  Q Light Controller Plus - Unit test
*/

#ifndef MATRIXSCRIPTFLOATCONTROLS_TEST_H
#define MATRIXSCRIPTFLOATCONTROLS_TEST_H

#include <QObject>

class MatrixScriptFloatControls_Test final : public QObject
{
    Q_OBJECT

private slots:
    void editorFloatBounds_data();
    void editorFloatBounds();
    void validatorTextPath_data();
    void validatorTextPath();
    void unboundedValidatorTextPath_data();
    void unboundedValidatorTextPath();
    void unboundedQmlReceiver_data();
    void unboundedQmlReceiver();
    void constructionBelowMinimumKeepsSync();
    void unboundedPreservesLegacyBehavior();
    void invalidBoundsPreservePropertyAndAuthoredValue();
    void boundedFloatSaveLoadRoundTrip();
    void editorAuthoredFloatValue_data();
    void editorAuthoredFloatValue();
};

#endif // MATRIXSCRIPTFLOATCONTROLS_TEST_H
