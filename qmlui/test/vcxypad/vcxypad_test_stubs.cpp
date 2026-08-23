/*
  Q Light Controller Plus - Unit test stubs

  VCXYPad references Tardis (undo/redo journal) and FixtureManager (QML
  groups tree) from code paths the range/head tests never enter. Providing
  the symbols here keeps the test from pulling in the whole QML application
  translation unit graph.

  Same pattern as mcp/test/functionmanager_stub.cpp: the classes are
  mirrored by name only, never constructed, and the bodies never run.

  Licensed under the Apache License, Version 2.0
*/

#include <QList>
#include <QString>
#include <QVariant>

class Doc;
class TreeModel;
class SceneValue;

class Tardis
{
public:
    static Tardis *instance();
    void enqueueAction(int code, unsigned int objID, QVariant oldVal, QVariant newVal);
};

Tardis *Tardis::instance()
{
    // Only reached by geometry/position/preset paths, which these tests
    // don't exercise.
    return nullptr;
}

void Tardis::enqueueAction(int, unsigned int, QVariant, QVariant)
{
}

class FixtureManager
{
public:
    static void updateGroupsTree(Doc *doc, TreeModel *treeModel, QString searchFilter,
                                 int showFlags, QList<SceneValue> checkedChannels);
};

void FixtureManager::updateGroupsTree(Doc *, TreeModel *, QString, int, QList<SceneValue>)
{
    // Only reached by VCXYPad::groupsTreeModel(), which is QML-only.
}
