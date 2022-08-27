/*
* KDiff3 - Text Diff And Merge Tool
*
* SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
* SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
* SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef MERGERESULTWINDOW_H
#define MERGERESULTWINDOW_H

#include "EXTRACT/2_final/MergeEditLine.h"
#include "EXTRACT/2_final/diff.h"
#include "FileNameLineEdit.h"
#include "Overview.h"



#include "EXTRACT/my/MergeDataObj.h"


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





// saveDocument() вызывается по кнопке сохранить - там поидее считываются все важные данные которые формируются при мерже. Ну или само содержимое файла, а оно уже менятся исходя из нужных данных
// Однако перед тем как делать сохранение там надо пофиксить все мерж конфликты. Это делается.... через ui
//
// getNrOfUnsolvedConflicts() - получить кол-во конфликтов. Не дает сохранить документ пока они есть
//
// В init() происходит инциализация и внутри него сам мерж




// Думаю можно начать исследовать приватные переменные и смотреть какие где юзаются и зачем.
// и для каждой написать - это связано с данными или UI
// В итоге все что про данные оставить а для UI удалить и исходя из этого будет видно какие методы зависят только от UI - их удалить.
// А те что зависят от данных и от UI - рефакторить (так чтобы из итогового окошка вызывать эти зарефакторенные методы, которые уже будут только от данных зависеть)
//
// Хорошая тактика тут будет - сделать сначала копию этого класса (но для MergeResultWindow) и засунуть туда ТОЛЬКО данные, без методов (для этого надо четко понять что данные а что нет)
// как минимум - m_mergeLineList точно туда. Дальше - заменить использование переменных с данными во всем MergeResultWindow на этот класс (его поля публичные - эти данные должны быть публичными).
// А затем постепенно переносить методы которые относятся только к данным внутрь этого своего класса и убирать из окошка эти методы.
// А те методы которые не получается перетащить, т.к. там зависимости на UI и события UI - их рефакторить.
// В итоге у меня основная логика будет сосредоточена в этом классе + видны будут использования из окошка. Там будут разные грязные использования переменных напрямую
// если пойму как этого избежать - буду вытаскивать в методы такое без зависимостей на UI.




// Выделить все что нужно для операции merge - входные и выходные данные. Оставить их тут и убрать зависимость на UI (но можно исопльзовать внешний KDiff3App)
// далее заюзать этот класс как посредник вместо аналогичного кода в исходном MergeResultWindow
// таким образом логика мержа останется тут + я смогу проверить что все работае ткак раньше и буду хорошо видеть зависимости.
//
// Такое же упражнение потом проделать для KDiff3App и тут заиспользовать потом вместо KDiff3App этот новый класс с логикой, т.к. тут поидее ничего кроме логики оттуда вероятно не будет.











// Илья: вынес это, т.к. там внутри есть MergeResultWindow::slotAutoSolve() в котором есть логика мержа.
// Нужно эту логику потом вынести из UI части
//
// Это окошко с результатами мержа - часть основного UI. Это окошко снизу где мерж вручную меняется и резолвится.
class MergeResultWindow: public QWidget
{
   Q_OBJECT
 public:
   // Объект данных который я сделал - там содержится все завязанное на данных вытащенное из этого классса с окошком
   MergeDataObj* m_pMergeDataObj = nullptr;





   static QScrollBar* mVScrollBar;

   // DONE
   // Отсюда в данные передаются только опции - они копируются в виде части содержащей только данные и передаются в выделенный объект для работы с данными
   MergeResultWindow(MergeDataObj* pMergeDataObj, QWidget* pParent, const QSharedPointer<Options>& pOptions, QStatusBar* pStatusBar);

   // DONE
   // Я его сам завел чтобы удалять выделенную память
   ~MergeResultWindow();

   // DONE
   // Инициализация данных вызывается там внутри из выделенного объекта
   void init(
       const QVector<LineData>* pLineDataA, LineRef sizeA,
       const QVector<LineData>* pLineDataB, LineRef sizeB,
       const QVector<LineData>* pLineDataC, LineRef sizeC,
       const Diff3LineList* pDiff3LineList,
       TotalDiffStatus* pTotalDiffStatus,
       bool bAutoSolve
   );

   void setupConnections(const KDiff3App* app);

   static void initActions(KActionCollection* ac);

   void connectActions() const;

   // DONT
   // Выделал ресет данных в отдельный объект (используетя внутри)
   void reset();

   // DONE
   // Выделил данные в отдельный объект а UI часть и само сохранение файла на диск оставил тут, в окошке
   bool saveDocument(const QString& fileName, QTextCodec* pEncoding, e_LineEndStyle eLineEndStyle);


   // DONE
   void choose(e_SrcSelector selector);


   void chooseGlobal(e_SrcSelector selector, bool bConflictsOnly, bool bWhiteSpaceOnly);

   int getMaxTextWidth();         // width of longest text line
   int getNofLines() const;
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
   bool findString(const QString& s, LineRef& d3vLine, int& posInLine, bool bDirDown, bool bCaseSensitive);
   void setSelection(int firstLine, int startPos, int lastLine, int endPos);
   e_OverviewMode getOverviewMode();

   void slotUpdateAvailabilities();

 public Q_SLOTS:
   void setOverviewMode(e_OverviewMode eOverviewMode);
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
   void slotSplitDiff(LineIndex firstD3lLineIdx, LineIndex lastD3lLineIdx);
   void slotJoinDiffs(LineIndex firstD3lLineIdx, LineIndex lastD3lLineIdx);
   void slotSetFastSelectorLine(LineIndex);
   void setPaintingAllowed(bool);
   void updateSourceMask();
   void slotStatusMessageChanged(const QString&);

   void slotChooseAEverywhere() { chooseGlobal(e_SrcSelector::A, false, false); }
   void slotChooseBEverywhere() { chooseGlobal(e_SrcSelector::B, false, false); }
   void slotChooseCEverywhere() { chooseGlobal(e_SrcSelector::C, false, false); }
   void slotChooseAForUnsolvedConflicts() { chooseGlobal(e_SrcSelector::A, true, false); }
   void slotChooseBForUnsolvedConflicts() { chooseGlobal(e_SrcSelector::B, true, false); }
   void slotChooseCForUnsolvedConflicts() { chooseGlobal(e_SrcSelector::C, true, false); }
   void slotChooseAForUnsolvedWhiteSpaceConflicts() { chooseGlobal(e_SrcSelector::A, true, true); }
   void slotChooseBForUnsolvedWhiteSpaceConflicts() { chooseGlobal(e_SrcSelector::B, true, true); }
   void slotChooseCForUnsolvedWhiteSpaceConflicts() { chooseGlobal(e_SrcSelector::C, true, true); }
   void slotRefresh();

   void slotResize();

   void slotCut();

   void slotCopy();

   void slotSelectAll();

   void scrollVertically(QtNumberType deltaY);

 Q_SIGNALS:
   void statusBarMessage(const QString& message);
   void scrollMergeResultWindow(int deltaX, int deltaY);
   void modifiedChanged(bool bModified);
   void setFastSelectorRange(LineRef line1, LineCount nofLines);
   void sourceMask(int srcMask, int enabledMask);
   void resizeSignal();
   void selectionEnd();
   void newSelection();
   void updateAvailabilities();
   void showPopupMenu(const QPoint& point);
   void noRelevantChangesDetected();

 private:
   // DONE
   void merge(bool bAutoSolve, e_SrcSelector defaultSelector, bool bConflictsOnly = false, bool bWhiteSpaceOnly = false);

   QString getString(int lineIdx);
   void showUnsolvedConflictsStatusMessage();

   static QPointer<QAction> chooseAEverywhere;
   static QPointer<QAction> chooseBEverywhere;
   static QPointer<QAction> chooseCEverywhere;
   static QPointer<QAction> chooseAForUnsolvedConflicts;
   static QPointer<QAction> chooseBForUnsolvedConflicts;
   static QPointer<QAction> chooseCForUnsolvedConflicts;
   static QPointer<QAction> chooseAForUnsolvedWhiteSpaceConflicts;
   static QPointer<QAction> chooseBForUnsolvedWhiteSpaceConflicts;
   static QPointer<QAction> chooseCForUnsolvedWhiteSpaceConflicts;

   QSharedPointer<Options> m_pOptions = nullptr;
   MyOptions* m_pMyOptions;

   int m_delayedDrawTimer = 0;
   e_OverviewMode mOverviewMode;
   QString m_persistentStatusMessage;

 private:
   struct HistoryMapEntry {
       MergeEditLineList mellA;
       MergeEditLineList mellB;
       MergeEditLineList mellC;
       MergeEditLineList& choice(bool bThreeInputs);
       bool staysInPlace(bool bThreeInputs, Diff3LineList::const_iterator& iHistoryEnd);
   };
   typedef std::map<QString, HistoryMapEntry> HistoryMap;
   void collectHistoryInformation(e_SrcSelector src, Diff3LineList::const_iterator& iHistoryBegin, Diff3LineList::const_iterator& iHistoryEnd, HistoryMap& historyMap, std::list<HistoryMap::iterator>& hitList);


   int m_currentPos;
   bool checkOverviewIgnore(MergeLineList::iterator& i);

   enum e_Direction
   {
       eUp,
       eDown
   };
   enum e_EndPoint
   {
       eDelta,
       eConflict,
       eUnsolvedConflict,
       eLine,
       eEnd
   };
   void go(e_Direction eDir, e_EndPoint eEndPoint);
   bool calcIteratorFromLineNr(
       int line,
       MergeLineList::iterator& mlIt,
       MergeEditLineList::iterator& melIt);
   MergeLineList::iterator splitAtDiff3LineIdx(int d3lLineIdx);

   void paintEvent(QPaintEvent* e) override;

   int getTextXOffset();
   QVector<QTextLayout::FormatRange> getTextLayoutForLine(int line, const QString& s, QTextLayout& textLayout);
   void myUpdate(int afterMilliSecs);
   void timerEvent(QTimerEvent*) override;
   void writeLine(
       RLPainter& p, int line, const QString& str,
       enum e_SrcSelector srcSelect, e_MergeDetails mergeDetails, int rangeMark, bool bUserModified, bool bLineRemoved, bool bWhiteSpaceConflict
   );
   void setFastSelector(MergeLineList::iterator i);
   LineRef convertToLine(QtNumberType y);
   bool event(QEvent*) override;
   void mousePressEvent(QMouseEvent* e) override;
   void mouseDoubleClickEvent(QMouseEvent* e) override;
   void mouseReleaseEvent(QMouseEvent*) override;
   void mouseMoveEvent(QMouseEvent*) override;
   void resizeEvent(QResizeEvent* e) override;
   void keyPressEvent(QKeyEvent* e) override;
   void wheelEvent(QWheelEvent* pWheelEvent) override;
   void focusInEvent(QFocusEvent* e) override;

   bool canCut() { return hasFocus() && !getSelection().isEmpty(); }
   bool canCopy() { return hasFocus() && !getSelection().isEmpty(); }

   QPixmap m_pixmap;
   LineRef m_firstLine = 0;
   int m_horizScrollOffset = 0;
   LineCount m_nofLines = 0;
   int m_maxTextWidth = -1;
   bool m_bMyUpdate = false;
   bool m_bInsertMode = true;
   bool m_bModified = false;
   void setModified(bool bModified = true);




   // По названиям похоже что эта пачка полей - UI
   int m_scrollDeltaX = 0;
   int m_scrollDeltaY = 0;
   int m_cursorXPos = 0;
   int m_cursorXPixelPos;
   int m_cursorYPos = 0;
   int m_cursorOldXPixelPos = 0;
   bool m_bCursorOn = true; // blinking on and off each second
   QTimer m_cursorTimer;
   bool m_bCursorUpdate = false;
   QStatusBar* m_pStatusBar;






   Selection m_selection;

   bool deleteSelection2(QString& str, int& x, int& y,
                         MergeLineList::iterator& mlIt, MergeEditLineList::iterator& melIt);
   bool doRelevantChangesExist();

   /*
     This list exists solely to auto disconnect boost signals.
   */
   std::list<boost::signals2::scoped_connection> connections;
 public Q_SLOTS:
   void deleteSelection();
   void pasteClipboard(bool bFromSelection);
 private Q_SLOTS:
   void slotCursorUpdate();
};

#endif
