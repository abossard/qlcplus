/*
  Q Light Controller Plus - Unit test
  audioprofile_test.h

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef AUDIOPROFILE_TEST_H
#define AUDIOPROFILE_TEST_H

#include <QObject>

class AudioProfile_Test final : public QObject
{
    Q_OBJECT

private slots:
    void testCreateProfile();
    void testXmlRoundTrip();
    void testLegacyMigration();
    void testDocRegistration();
    void testEnsureDefault();
    void testVersionValidation_data();
    void testVersionValidation();
};

#endif // AUDIOPROFILE_TEST_H
