#ifndef KDIFF3_MAINDATAOBJ_H
#define KDIFF3_MAINDATAOBJ_H

// Эти инклюды и предефайны взял с KDiff3App (не факт что они все нужны) просто чтобы потом не париться при реализации.
// Потом в итоге нужно будет поубирать ненужное
// --------------------------------------------------------------------

#include "EXTRACT/my/MyOptions.h"
#include "EXTRACT/2_final/defmac.h"
#include "EXTRACT/2_final/diff.h"
#include "combiners.h"

#include <boost/signals2.hpp>

// include files for Qt
#include <QAction>
#include <QApplication>
#include <QEventLoop>
#include <QPointer>
#include <QScrollBar>
#include <QSplitter>

// include files for KDE
#include <KConfigGroup>
#include <KMainWindow>
#include <KParts/MainWindow>
#include <KSharedConfig>
#include <KToggleAction>

class MainDataObj;

// forward declaration of the KDiff3 classes
class OptionDialog;

class Overview;
enum class e_OverviewMode;
class FindDialog;
//class ManualDiffHelpDialog;
class DiffTextWindow;
class DiffTextWindowFrame;
class MergeDataObj;
class MergeResultWindow;
class WindowTitleWidget;

class QStatusBar;
class QMenu;

class KToggleAction;
class KToolBar;
class KActionCollection;

// --------------------------------------------------------------------

class MainDataObj
{
  public:
    MainDataObj(MyOptions* pMyOptions);

    // Данные пока в public пока в процессе рефакторинга потом перенести в private что нужно
  public:
    TotalDiffStatus *m_totalDiffStatus = new TotalDiffStatus();

    // Илья: это сами исходные файлы
    QSharedPointer<SourceData> m_sd1 = QSharedPointer<SourceData>::create();
    QSharedPointer<SourceData> m_sd2 = QSharedPointer<SourceData>::create();
    QSharedPointer<SourceData> m_sd3 = QSharedPointer<SourceData>::create();

    // Илья: это походу выходное имя файла куда скидывается результат (он вроде не считывается, только перезаписывается)
    QString m_outputFilename;

    bool m_bDefaultFilename;

    // Илья: это вроде собранные вариации диффов между 1 2 и 3 исходными файлами. Заполняются вроде после вызова диффа
    DiffList m_diffList12;
    DiffList m_diffList23;
    DiffList m_diffList13;

    // Илья: Возможно это какие то временные данные нужные в процессе создания диффов
    QSharedPointer<DiffBufferInfo> m_diffBufferInfo = QSharedPointer<DiffBufferInfo>::create();
    Diff3LineList m_diff3LineList;
    Diff3LineVector m_diff3LineVector;
    ManualDiffHelpList m_manualDiffHelpList;
  private:
    MyOptions* m_pMyOptions;
};

#endif // KDIFF3_MAINDATAOBJ_H
