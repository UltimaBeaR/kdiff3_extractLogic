/**
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2021 Michael Reeves <reeves.87@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 */

#ifndef CVSIGNORELIST_H
#define CVSIGNORELIST_H

#include "EXTRACT/2_final/file_access/fileaccess.h"

#include "EXTRACT/2_final/file_access/IgnoreList.h"

#include <QString>
#include <QStringList>

class CvsIgnoreList: public IgnoreList
{
  private:
    inline const char* getVarName() const override { return "CVSIGNORE"; }
    inline const QString getIgnoreName() const override { return QStringLiteral(".cvsignore"); }
};

#endif /* CVSIGNORELIST_H */
