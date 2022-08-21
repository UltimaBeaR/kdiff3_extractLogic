/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef DIRECTORY_MERGE_WINDOW_H
#define DIRECTORY_MERGE_WINDOW_H

#include "EXTRACT/1_transitional/diff.h" //TotalDiffStatus
#include "common.h"
#include "fileaccess.h"

#include <QTreeWidget>
#include <QEvent>

#include <list>
#include <map>

class Options;
class StatusInfo;
class DirectoryMergeInfo;
class OneDirectoryInfo;
class QLabel;
class QAction;
class KToggleAction;
class KActionCollection;
class TotalDiffStatus;
class DirectoryInfo;

class MergeFileInfos;

class KDiff3App;
class DirectoryMergeWindow : public QTreeView
{
   Q_OBJECT
 public:
   struct t_ItemInfo;

   DirectoryMergeWindow(QWidget* pParent, const QSharedPointer<Options>& pOptions);
   ~DirectoryMergeWindow() override;
   void setDirectoryMergeInfo(DirectoryMergeInfo* p);
   bool init(
      const QSharedPointer<DirectoryInfo>& dirInfo,
      bool bDirectoryMerge,
      bool bReload = false
   );
   bool isFileSelected();
   bool isDirectoryMergeInProgress();
   int totalColumnWidth();
   bool isSyncMode();
   bool isScanning();
   void initDirectoryMergeActions(KDiff3App* pKDiff3App, KActionCollection* ac);

   void setupConnections(const KDiff3App* app);
   void updateAvailabilities(bool bMergeEditorVisible, bool bDirCompare, bool bDiffWindowVisible,
      KToggleAction* chooseA, KToggleAction* chooseB, KToggleAction* chooseC);
   void updateFileVisibilities();

   void mousePressEvent(QMouseEvent* e) override;
   void keyPressEvent(QKeyEvent* e) override;
   void focusInEvent(QFocusEvent* e) override;
   void focusOutEvent(QFocusEvent* e) override;
   void contextMenuEvent(QContextMenuEvent* e) override;

   QString getDirNameA() const;
   QString getDirNameB() const;
   QString getDirNameC() const;
   QString getDirNameDest() const;

 public Q_SLOTS:
   void reload();
   void mergeCurrentFile();
   void compareCurrentFile();
   void slotRunOperationForAllItems();
   void slotRunOperationForCurrentItem();
   void mergeResultSaved(const QString& fileName);
   void slotChooseAEverywhere();
   void slotChooseBEverywhere();
   void slotChooseCEverywhere();
   void slotAutoChooseEverywhere();
   void slotNoOpEverywhere();
   void slotFoldAllSubdirs();
   void slotUnfoldAllSubdirs();
   void slotShowIdenticalFiles();
   void slotShowDifferentFiles();
   void slotShowFilesOnlyInA();
   void slotShowFilesOnlyInB();
   void slotShowFilesOnlyInC();

   void slotSynchronizeDirectories();
   void slotChooseNewerFiles();

   void slotCompareExplicitlySelectedFiles();
   void slotMergeExplicitlySelectedFiles();

   // Merge current item (merge mode)
   void slotCurrentDoNothing();
   void slotCurrentChooseA();
   void slotCurrentChooseB();
   void slotCurrentChooseC();
   void slotCurrentMerge();
   void slotCurrentDelete();
   // Sync current item
   void slotCurrentCopyAToB();
   void slotCurrentCopyBToA();
   void slotCurrentDeleteA();
   void slotCurrentDeleteB();
   void slotCurrentDeleteAAndB();
   void slotCurrentMergeToA();
   void slotCurrentMergeToB();
   void slotCurrentMergeToAAndB();

   void slotSaveMergeState();
   void slotLoadMergeState();

   inline void slotRefresh() { updateFileVisibilities(); };

Q_SIGNALS:
   void startDiffMerge(QStringList &errors, const QString& fn1, const QString& fn2, const QString& fn3, const QString& ofn, const QString&, const QString&, const QString&, TotalDiffStatus*);
   void updateAvailabilities();
   void statusBarMessage(const QString& msg);
protected Q_SLOTS:
   void onDoubleClick(const QModelIndex&);
   void onExpanded();
   void currentChanged(const QModelIndex& current, const QModelIndex& previous) override; // override
private:
  int getIntFromIndex(const QModelIndex& index) const;
  const QSharedPointer<Options>& getOptions() const;

  class DirectoryMergeWindowPrivate;
  DirectoryMergeWindowPrivate* d;
  class DirMergeItemDelegate;
};

class DirectoryMergeInfo : public QFrame
{
   Q_OBJECT
public:
   explicit DirectoryMergeInfo(QWidget* pParent);
   void setInfo(
      const FileAccess& dirA,
      const FileAccess& dirB,
      const FileAccess& dirC,
      const FileAccess& dirDest,
      MergeFileInfos& mfi);
   QTreeWidget* getInfoList() { return m_pInfoList; }
   bool eventFilter(QObject* o, QEvent* e) override;
Q_SIGNALS:
   void gotFocus();

private:
   void addListViewItem(const QString& dir, const QString& basePath, FileAccess* fi);

   QLabel* m_pInfoA;
   QLabel* m_pInfoB;
   QLabel* m_pInfoC;
   QLabel* m_pInfoDest;

   QLabel* m_pA;
   QLabel* m_pB;
   QLabel* m_pC;
   QLabel* m_pDest;

   QTreeWidget* m_pInfoList;
};

#endif
