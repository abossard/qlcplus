/*
  Q Light Controller Plus
  huescriptscache.h

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

#ifndef HUESCRIPTSCACHE_H
#define HUESCRIPTSCACHE_H

#include <QStringList>
#include <QMap>

class HUEScript;
class QDir;
class Doc;

/** @addtogroup engine Engine
 * @{
 */

/**
 * Cache of the HSV-contract scripts available to a HUEMatrix.
 *
 * A HUEMatrix can run every upstream RGB script *and* the HSV scripts shipped
 * in the huescripts directory, so this cache indexes both. A plain RGBMatrix
 * only ever sees RGBScriptsCache, which never indexes the huescripts
 * directory.
 */
class HUEScriptsCache final
{
public:
    explicit HUEScriptsCache(Doc *doc);

    /** Return the names of every cached script, HSV and RGB alike. */
    QStringList names() const;

    /** Return the names of the HSV-contract scripts only. */
    QStringList hsvNames() const;

    /** Get a script instance by name, or NULL if $name is unknown. */
    HUEScript *script(QString name) const;

    /**
     * Load scripts from $dir. When $hsvContract is true the scripts are
     * expected to return a flat Float32Array of HSV triples; otherwise the
     * upstream nested packed-uint contract is assumed.
     *
     * Returns true if $dir was accessible, even when it holds no script.
     */
    bool load(const QDir &dir, bool hsvContract);

    /** Default system directory holding the installed HSV scripts. */
    static QDir systemScriptsDirectory();

    /** User directory holding custom HSV scripts. */
    static QDir userScriptsDirectory();

    /** Directories the HSV-contract scripts were actually loaded from. The
     *  JS shims (hsvutil.js) live alongside them. */
    static QStringList hsvScriptDirectories();

private:
    Doc *m_doc;
    QMap<QString, QString> m_scriptsMap;    //! name -> absolute file name
    QStringList m_hsvNames;                 //! names loaded with the HSV contract

    static QStringList s_hsvDirectories;    //! dirs that yielded HSV scripts
};

/** @} */

#endif // HUESCRIPTSCACHE_H
