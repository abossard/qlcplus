/*
  Q Light Controller Plus - Unit test
  vc_query_filter_test.h

  Copyright (C) Massimo Callegari

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

#ifndef VC_QUERY_FILTER_TEST_H
#define VC_QUERY_FILTER_TEST_H

#include <QObject>

class VCQueryFilter_Test final : public QObject
{
    Q_OBJECT

private slots:
    // Argument validation — parameterized
    void validateArgs_data();
    void validateArgs();

    // Glob matching — parameterized
    void globMatch_data();
    void globMatch();

    // Widget filtering — parameterized
    void filterWidget_data();
    void filterWidget();

    // Field selection / serialization — parameterized
    void serializeWidget_data();
    void serializeWidget();

    // Known properties completeness
    void knownPropertiesCompleteness();

    // Compound group expansion
    void compoundGroupExpansion();
};

#endif // VC_QUERY_FILTER_TEST_H
