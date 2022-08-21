/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef WINDOWTITLEWIDGET_H
#define WINDOWTITLEWIDGET_H

#include "EXTRACT/2_final/diff.h"
#include "FileNameLineEdit.h"
#include "MergeEditLine.h"
#include "Overview.h"

#include "selection.h"

#include <boost/signals2.hpp>

#include <QLineEdit>
#include <QPointer>
#include <QStatusBar>
#include <QTextLayout>
#include <QTimer>
#include <QWidget>

class QPainter;
class RLPainter;
class QScrollBar;
class KActionCollection;
class KToggleAction;

class KDiff3App;

class QLineEdit;
class QTextCodec;
class QComboBox;
class QLabel;

class WindowTitleWidget: public QWidget
{
    Q_OBJECT
  private:
    QLabel*      m_pLabel;
    FileNameLineEdit*   m_pFileNameLineEdit;
    //QPushButton* m_pBrowseButton;
    QLabel*      m_pModifiedLabel;
    QLabel*      m_pLineEndStyleLabel;
    QComboBox*   m_pLineEndStyleSelector;
    QLabel*      m_pEncodingLabel;
    QComboBox*   m_pEncodingSelector;
    QSharedPointer<Options> m_pOptions;

  public:
    explicit WindowTitleWidget(const QSharedPointer<Options>& pOptions);
    QTextCodec* getEncoding();
    void setFileName(const QString& fileName);
    QString getFileName();
    void setEncodings(QTextCodec* pCodecForA, QTextCodec* pCodecForB, QTextCodec* pCodecForC);
    void setEncoding(QTextCodec* pEncoding);
    void setLineEndStyles(e_LineEndStyle eLineEndStyleA, e_LineEndStyle eLineEndStyleB, e_LineEndStyle eLineEndStyleC);
    e_LineEndStyle getLineEndStyle();

    bool eventFilter(QObject* o, QEvent* e) override;
  public Q_SLOTS:
    void slotSetModified(bool bModified);
    //private Q_SLOTS:
    //   void slotBrowseButtonClicked();
};

#endif
