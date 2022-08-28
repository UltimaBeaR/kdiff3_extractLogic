#include "MainDataObj.h"

// Эти инклюды взял с KDiff3App - взял смесь двух cpp файлов в реализациях (не факт что они все нужны) просто чтобы потом не париться при реализации.
// Потом в итоге нужно будет поубирать ненужное
// --------------------------------------------------------------------

#include "DirectoryInfo.h"
#include "EXTRACT/2_final/Logging.h"
#include "EXTRACT/0_consider/ui/MergeResultWindow.h"
#include "EXTRACT/my/MergeDataObj.h"
#include "EXTRACT/2_final/file_access/fileaccess.h"
#include "EXTRACT/2_final/defmac.h"
#include "EXTRACT/2_final/ui/progress.h"
#include "EXTRACT/2_final/utils/Utils.h"
#include "RLPainter.h"
#include "WindowTitleWidget.h"
#include "difftextwindow.h"
#include "directorymergewindow.h"
#include "guiutils.h"
#include "kdiff3_part.h"
#include "kdiff3_shell.h"
#include "optiondialog.h"
#include "smalldialogs.h"

#ifndef Q_OS_WIN
#include <unistd.h>
#endif

#include <algorithm>
#include <list>

// include files for QT
#include <QClipboard>
#include <QCheckBox>
#include <QCommandLineParser>
#include <QDesktopWidget>
#include <QDir>
#include <QFileDialog>
#include <QLayout>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QPaintDevice>
#include <QPainter>
#include <QPointer>
#include <QPrintDialog>
#include <QPrinter>
#include <QSplitter>
#include <QStatusBar>
#include <QTextEdit>
#include <QTextStream>
#include <QUrl>
// include files for KDE
#include <KCrash>
#include <KLocalizedString>
#include <KMessageBox>
#include <KStandardAction>
#include <KActionCollection>
#include <KToggleAction>
#include <KToolBar>
#include <QDialog>
#include <QEvent> // QKeyEvent, QDropEvent, QInputEvent
#include <QMimeData>
#include <QProcess>
#include <QScrollBar>
#include <QStringList>
#include <QTextCodec>
#include <KShortcutsDialog>

// --------------------------------------------------------------------

MainDataObj::MainDataObj()
{
}

void MainDataObj::init(
    MyOptions* pMyOptions,
    bool hasArgs,
    bool autoFlag,
    QString output,
    bool merge,
    bool hasFileName1,
    QString fileName1,
    bool hasFileName2,
    QString fileName2,
    bool hasFileName3,
    QString fileName3,
    QString base,
    QStringList fName,
    QString l1,
    QString l2,
    QString l3
)
{
    m_pMyOptions = pMyOptions;

    m_bAutoMode = autoFlag || m_pMyOptions->autoSaveAndQuitOnMergeWithoutConflicts();

    if(hasArgs) {
        m_outputFilename = output;

        if(!m_outputFilename.isEmpty())
            m_outputFilename = FileAccess(m_outputFilename, true).absoluteFilePath();

        if(m_bAutoMode && m_outputFilename.isEmpty())
        {
            if(autoFlag)
            {
                QTextStream(stderr) << i18n("Option --auto used, but no output file specified.") << "\n";
            }
            m_bAutoMode = false;
        }

        if(m_outputFilename.isEmpty() && merge)
        {
            m_outputFilename = "unnamed.txt";
            m_bDefaultFilename = true;
        }
        else
        {
            m_bDefaultFilename = false;
        }

        m_sd1->setFilename(base);
        if(m_sd1->isEmpty()) {
            if(hasFileName1) m_sd1->setFilename(fileName1);
            if(hasFileName2) m_sd2->setFilename(fileName2);
            if(hasFileName3) m_sd3->setFilename(fileName3);
        }
        else
        {
            if(hasFileName1) m_sd2->setFilename(fileName1);
            if(hasFileName2) m_sd3->setFilename(fileName2);
        }
        //Set m_bDirCompare flag
        m_bDirCompare = m_sd1->isDir();

        QStringList aliasList = fName;
        QStringList::Iterator ali = aliasList.begin();

        QString an1 = l1;
        if(!an1.isEmpty()) {
            m_sd1->setAliasName(an1);
        }
        else if(ali != aliasList.end())
        {
            m_sd1->setAliasName(*ali);
            ++ali;
        }

        QString an2 = l2;
        if(!an2.isEmpty()) {
            m_sd2->setAliasName(an2);
        }
        else if(ali != aliasList.end())
        {
            m_sd2->setAliasName(*ali);
            ++ali;
        }

        QString an3 = l3;
        if(!an3.isEmpty()) {
            m_sd3->setAliasName(an3);
        }
        else if(ali != aliasList.end())
        {
            m_sd3->setAliasName(*ali);
            ++ali;
        }
    }
    else
    {
        m_bDefaultFilename = false;
    }
}