/***************************************************************************
 *   Copyright (C) 2002-2007 by Joachim Eibl <joachim.eibl at gmx.de>      *
 *   Copyright (C) 2018 Michael Reeves reeves.87@gmail.com                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef MERGERESULTWINDOW_H
#define MERGERESULTWINDOW_H

#include "diff.h"
#include "selection.h"
#include "Overview.h"
#include "FileNameLineEdit.h"
#include "MergeEditLine.h"

#include <QWidget>
#include <QTimer>
#include <QStatusBar>
#include <QTextLayout>
#include <QLineEdit>

class QPainter;
class RLPainter;
class KActionCollection;
class KToggleAction;
class MergeResultWindow : public QWidget
{
   Q_OBJECT
public:
   MergeResultWindow(QWidget* pParent, Options* pOptions, QStatusBar* pStatusBar);

   void init(
      const QVector<LineData>* pLineDataA, LineRef sizeA,
      const QVector<LineData>* pLineDataB, LineRef sizeB,
      const QVector<LineData>* pLineDataC, LineRef sizeC,
      const Diff3LineList* pDiff3LineList,
      TotalDiffStatus* pTotalDiffStatus
      );
   void initActions(KActionCollection* ac);
   void reset();

   bool saveDocument( const QString& fileName, QTextCodec* pEncoding, e_LineEndStyle eLineEndStyle );
   int getNrOfUnsolvedConflicts(int* pNrOfWhiteSpaceConflicts=nullptr);
   void choose(e_SrcSelector selector);
   void chooseGlobal(e_SrcSelector selector, bool bConflictsOnly, bool bWhiteSpaceOnly );

   int getMaxTextWidth();     // width of longest text line
   int getNofLines();
   int getVisibleTextAreaWidth(); // text area width without the border
   int getNofVisibleLines();
   QString getSelection();
   void resetSelection();
   void showNrOfConflicts();
   bool isDeltaAboveCurrent();
   bool isDeltaBelowCurrent();
   bool isConflictAboveCurrent();
   bool isConflictBelowCurrent();
   bool isUnsolvedConflictAtCurrent();
   bool isUnsolvedConflictAboveCurrent();
   bool isUnsolvedConflictBelowCurrent();
   bool findString( const QString& s, LineRef& d3vLine, int& posInLine, bool bDirDown, bool bCaseSensitive );
   void setSelection( int firstLine, int startPos, int lastLine, int endPos );
   void setOverviewMode( Overview::e_OverviewMode eOverviewMode );
   Overview::e_OverviewMode getOverviewMode();

   void slotUpdateAvailabilities(const bool bMergeEditorVisible, const bool m_bTripleDiff);

public Q_SLOTS:
   void setFirstLine(QtNumberType firstLine);
   void setHorizScrollOffset(int horizScrollOffset);

   void slotGoCurrent();
   void slotGoTop();
   void slotGoBottom();
   void slotGoPrevDelta();
   void slotGoNextDelta();
   void slotGoPrevUnsolvedConflict();
   void slotGoNextUnsolvedConflict();
   void slotGoPrevConflict();
   void slotGoNextConflict();
   void slotAutoSolve();
   void slotUnsolve();
   void slotMergeHistory();
   void slotRegExpAutoMerge();
   void slotSplitDiff( LineIndex firstD3lLineIdx, LineIndex lastD3lLineIdx );
   void slotJoinDiffs( LineIndex firstD3lLineIdx, LineIndex lastD3lLineIdx );
   void slotSetFastSelectorLine(LineIndex);
   void setPaintingAllowed(bool);
   void updateSourceMask();
   void slotStatusMessageChanged( const QString& );

   void slotChooseAEverywhere() { chooseGlobal(A, false, false); }
   void slotChooseBEverywhere() { chooseGlobal(B, false, false); }
   void slotChooseCEverywhere() { chooseGlobal(C, false, false); }
   void slotChooseAForUnsolvedConflicts() { chooseGlobal(A, true, false); }
   void slotChooseBForUnsolvedConflicts() { chooseGlobal(B, true, false); }
   void slotChooseCForUnsolvedConflicts() { chooseGlobal(C, true, false); }
   void slotChooseAForUnsolvedWhiteSpaceConflicts() { chooseGlobal(A, true, true); }
   void slotChooseBForUnsolvedWhiteSpaceConflicts() { chooseGlobal(B, true, true); }
   void slotChooseCForUnsolvedWhiteSpaceConflicts() { chooseGlobal(C, true, true); }

 Q_SIGNALS:
   void scrollMergeResultWindow( int deltaX, int deltaY );
   void modifiedChanged(bool bModified);
   void setFastSelectorRange( LineRef line1, LineCount nofLines );
   void sourceMask( int srcMask, int enabledMask );
   void resizeSignal();
   void selectionEnd();
   void newSelection();
   void updateAvailabilities();
   void showPopupMenu( const QPoint& point );
   void noRelevantChangesDetected();

private:
   void merge(bool bAutoSolve, e_SrcSelector defaultSelector, bool bConflictsOnly=false, bool bWhiteSpaceOnly=false );
   QString getString( int lineIdx );

   QAction* chooseAEverywhere = nullptr;
   QAction* chooseBEverywhere = nullptr;
   QAction* chooseCEverywhere = nullptr;
   QAction* chooseAForUnsolvedConflicts = nullptr;
   QAction* chooseBForUnsolvedConflicts = nullptr;
   QAction* chooseCForUnsolvedConflicts = nullptr;
   QAction* chooseAForUnsolvedWhiteSpaceConflicts = nullptr;
   QAction* chooseBForUnsolvedWhiteSpaceConflicts = nullptr;
   QAction* chooseCForUnsolvedWhiteSpaceConflicts = nullptr;

   Options* m_pOptions = nullptr;

   const QVector<LineData>* m_pldA;
   const QVector<LineData>* m_pldB;
   const QVector<LineData>* m_pldC;
   LineRef m_sizeA;
   LineRef m_sizeB;
   LineRef m_sizeC;

   const Diff3LineList* m_pDiff3LineList;
   TotalDiffStatus* m_pTotalDiffStatus;

   int m_delayedDrawTimer;
   Overview::e_OverviewMode m_eOverviewMode;
   QString m_persistentStatusMessage;
   void showUnsolvedConflictsStatusMessage();

private:
  static bool sameKindCheck(const MergeLine& ml1, const MergeLine& ml2);
  struct HistoryMapEntry {
      MergeEditLineList mellA;
      MergeEditLineList mellB;
      MergeEditLineList mellC;
      MergeEditLineList& choice( bool bThreeInputs );
      bool staysInPlace( bool bThreeInputs, Diff3LineList::const_iterator& iHistoryEnd );
   };
   typedef std::map<QString,HistoryMapEntry> HistoryMap;
   void collectHistoryInformation( e_SrcSelector src, Diff3LineList::const_iterator &iHistoryBegin, Diff3LineList::const_iterator &iHistoryEnd, HistoryMap& historyMap, std::list< HistoryMap::iterator >& hitList );

   MergeLineList m_mergeLineList;
   MergeLineList::iterator m_currentMergeLineIt;
   bool isItAtEnd( bool bIncrement, MergeLineList::iterator i )
   {
      if ( bIncrement ) return i!=m_mergeLineList.end();
      else              return i!=m_mergeLineList.begin();
   }

   int m_currentPos;
   bool checkOverviewIgnore(MergeLineList::iterator &i);

   enum e_Direction { eUp, eDown };
   enum e_EndPoint  { eDelta, eConflict, eUnsolvedConflict, eLine, eEnd };
   void go( e_Direction eDir, e_EndPoint eEndPoint );
   void calcIteratorFromLineNr(
      int line,
      MergeLineList::iterator& mlIt,
      MergeEditLineList::iterator& melIt
      );
   MergeLineList::iterator splitAtDiff3LineIdx( int d3lLineIdx );

   void paintEvent( QPaintEvent* e ) override;

   int getTextXOffset();
   QVector<QTextLayout::FormatRange> getTextLayoutForLine(int line, const QString& s, QTextLayout& textLayout );
   void myUpdate(int afterMilliSecs);
   void timerEvent(QTimerEvent*) override;
   void writeLine(
      RLPainter& p, int line, const QString& str,
      int srcSelect, e_MergeDetails mergeDetails, int rangeMark, bool bUserModified, bool bLineRemoved, bool bWhiteSpaceConflict
      );
   void setFastSelector(MergeLineList::iterator i);
   LineRef convertToLine( QtNumberType y );
   bool event(QEvent*) override;
   void mousePressEvent ( QMouseEvent* e ) override;
   void mouseDoubleClickEvent ( QMouseEvent* e ) override;
   void mouseReleaseEvent ( QMouseEvent * ) override;
   void mouseMoveEvent ( QMouseEvent * ) override;
   void resizeEvent( QResizeEvent* e ) override;
   void keyPressEvent( QKeyEvent* e ) override;
   void wheelEvent( QWheelEvent* e ) override;
   void focusInEvent( QFocusEvent* e ) override;

   QPixmap m_pixmap;
   LineRef m_firstLine;
   int m_horizScrollOffset;
   LineCount m_nofLines;
   int m_maxTextWidth;
   bool m_bMyUpdate;
   bool m_bInsertMode;
   bool m_bModified;
   void setModified(bool bModified=true);

   int m_scrollDeltaX;
   int m_scrollDeltaY;
   int m_cursorXPos;
   int m_cursorXPixelPos;
   int m_cursorYPos;
   int m_cursorOldXPixelPos;
   bool m_bCursorOn; // blinking on and off each second
   QTimer m_cursorTimer;
   bool m_bCursorUpdate;
   QStatusBar* m_pStatusBar;

   Selection m_selection;

   bool deleteSelection2( QString& str, int& x, int& y,
                    MergeLineList::iterator& mlIt, MergeEditLineList::iterator& melIt );
   bool doRelevantChangesExist();
public Q_SLOTS:
   void deleteSelection();
   void pasteClipboard(bool bFromSelection);
private Q_SLOTS:
   void slotCursorUpdate();
};

class QLineEdit;
class QTextCodec;
class QComboBox;
class QLabel;

class WindowTitleWidget : public QWidget
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
   Options*     m_pOptions;
public:
   explicit WindowTitleWidget(Options* pOptions);
   QTextCodec* getEncoding();
   void        setFileName(const QString& fileName );
   QString     getFileName();
   void setEncodings( QTextCodec* pCodecForA, QTextCodec* pCodecForB, QTextCodec* pCodecForC );
   void setEncoding( QTextCodec* pEncoding );
   void setLineEndStyles( e_LineEndStyle eLineEndStyleA, e_LineEndStyle eLineEndStyleB, e_LineEndStyle eLineEndStyleC);
   e_LineEndStyle getLineEndStyle();

   bool eventFilter( QObject* o, QEvent* e ) override;
public Q_SLOTS:
   void slotSetModified( bool bModified );
//private Q_SLOTS:
//   void slotBrowseButtonClicked();
};

#endif

