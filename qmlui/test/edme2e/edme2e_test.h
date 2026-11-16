/*
  Q Light Controller Plus - Unit test
  edme2e_test.h

  Licensed under the Apache License, Version 2.0
*/

#ifndef EDME2E_TEST_H
#define EDME2E_TEST_H

#include <QObject>
#include <QTemporaryDir>

#include <memory>

class App;
class VdjBridgePlugin;

class EdmE2E_Test : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // VirtualDJ telemetry on the socket -> Perform plays the song Show ->
    // the Song Director block under the playhead -> DMX
    void looksFollowSongBlocks_data();
    void looksFollowSongBlocks();

private:
    QTemporaryDir m_root;
    std::unique_ptr<App> m_app;
    std::unique_ptr<VdjBridgePlugin> m_plugin;
};

#endif
