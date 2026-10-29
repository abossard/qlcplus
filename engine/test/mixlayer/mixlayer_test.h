/*
  Q Light Controller Plus - Unit test
  mixlayer_test.h

  Copyright (c) QLC+ contributors

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#ifndef MIXLAYER_TEST_H
#define MIXLAYER_TEST_H

#include <QObject>

class Function;
class Scene;
class Doc;

class MixLayer_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void faderLayerDecidesLtp_data();
    void faderLayerDecidesLtp();
    void sceneLayerFlipMidRun_data();
    void sceneLayerFlipMidRun();
    void childFollowsParent_data();
    void childFollowsParent();
    void sharedChildFollowsHighestParent_data();
    void sharedChildFollowsHighestParent();
    void manualStartCountsAsLayerZero_data();
    void manualStartCountsAsLayerZero();
    void flashIgnoresLastRunLayer_data();
    void flashIgnoresLastRunLayer();

private:
    Scene *createScene(uchar ltp, uchar htp);
    Function *createParent(const QString &type, quint32 childId);
    QByteArray writeUniverse();

private:
    Doc *m_doc;
    quint32 m_fixtureId;
};

#endif
