/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef UTILS_URLTOSTRING_H
#define UTILS_URLTOSTRING_H

#include <QChar>
#include <QFontMetrics>
#include <QString>
#include <QStringList>

namespace Utils {
/*
  QUrl::toLocalFile does some special handling for locally visable windows network drives.
  If QUrl::isLocal however it returns false we get an empty string back.
*/
QString urlToString(const QUrl& url);

} // namespace Utils

#endif
