/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "Utils_urlToString.h"

#include "EXTRACT/2_final/file_access/fileaccess.h"

#include <QHash>
#include <QRegularExpression>
#include <QString>

namespace Utils {

/*
    QUrl::toLocalFile does some special handling for locally visable windows network drives.
    If QUrl::isLocal however it returns false we get an empty string back.
*/
QString urlToString(const QUrl& url)
{
    if(!FileAccess::isLocal(url))
        return url.toString();

    QString result = url.toLocalFile();
    if(result.isEmpty())
        return url.path();

    return result;
}

} // namespace Utils