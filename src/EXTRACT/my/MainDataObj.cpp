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

void MainDataObj::mainInit(
    TotalDiffStatus* pTotalDiffStatus,
    ProgressProxy &pp,
    QStringList errors,
    bool bLoadFiles,
    bool bUseCurrentEncoding,
    IgnoreFlags eIgnoreFlags
    )
{
    auto options = m_pMyOptions->getOptions();

    if(bLoadFiles)
    {
        m_manualDiffHelpList.clear();

        if(m_sd3->isEmpty())
            pp.setMaxNofSteps(4); // Read 2 files, 1 comparison, 1 finediff
        else
            pp.setMaxNofSteps(9); // Read 3 files, 3 comparisons, 3 finediffs

        // First get all input data.
        pp.setInformation(i18n("Loading A"));
        qCInfo(kdiffMain) << i18n("Loading A: %1", m_sd1->getFilename());

        // Чтение файла 1
        if(bUseCurrentEncoding)
            m_sd1->readAndPreprocess(m_sd1->getEncoding(), false, m_pMyOptions);
        else
            m_sd1->readAndPreprocess(options->m_pEncodingA, options->m_bAutoDetectUnicodeA, m_pMyOptions);

        pp.step();

        pp.setInformation(i18n("Loading B"));
        qCInfo(kdiffMain) << i18n("Loading B: %1", m_sd2->getFilename());

        // Чтение файла 2
        if(bUseCurrentEncoding)
            m_sd2->readAndPreprocess(m_sd2->getEncoding(), false, m_pMyOptions);
        else
            m_sd2->readAndPreprocess(options->m_pEncodingB, options->m_bAutoDetectUnicodeB, m_pMyOptions);

        pp.step();
        errors.append(m_sd1->getErrors());
        errors.append(m_sd2->getErrors());
    }
    else
    {
        if(m_sd3->isEmpty())
            pp.setMaxNofSteps(2); // 1 comparison, 1 finediff
        else
            pp.setMaxNofSteps(6); // 3 comparisons, 3 finediffs
    }

    pTotalDiffStatus->reset();

    if(errors.isEmpty())
    {
        // Run the diff.

        // Кейс когда сравниваются 2 файла. Дифф делается один (между 2мя файлами) в этом случае
        if(m_sd3->isEmpty())
        {
            pTotalDiffStatus->setBinaryEqualAB(m_sd1->isBinaryEqualWith(m_sd2));

            if(m_sd1->isText() && m_sd2->isText())
            {
                pp.setInformation(i18n("Diff: A <-> B"));
                qCInfo(kdiffMain) << i18n("Diff: A <-> B");
                m_manualDiffHelpList.runDiff(m_sd1->getLineDataForDiff(), m_sd1->getSizeLines(), m_sd2->getLineDataForDiff(), m_sd2->getSizeLines(), m_diffList12, e_SrcSelector::A, e_SrcSelector::B,
                                             options);

                pp.step();

                pp.setInformation(i18n("Linediff: A <-> B"));
                qCInfo(kdiffMain) << i18n("Linediff: A <-> B");
                m_diff3LineList.calcDiff3LineListUsingAB(&m_diffList12);
                m_diff3LineList.debugLineCheck(m_sd1->getSizeLines(), e_SrcSelector::A);

                pTotalDiffStatus->setTextEqualAB(m_diff3LineList.fineDiff(e_SrcSelector::A, m_sd1->getLineDataForDisplay(), m_sd2->getLineDataForDisplay(), eIgnoreFlags));
                if(m_sd1->getSizeBytes() == 0) pTotalDiffStatus->setTextEqualAB(false);

                pp.step();
            }
            else
            {
                pp.step();
                pp.step();
            }
        }
        // Кейс когда сравниваются 3 файла
        else
        {
            if(bLoadFiles)
            {
                pp.setInformation(i18n("Loading C"));
                qCInfo(kdiffMain) << i18n("Loading C: %1", m_sd3->getFilename());

                // Чтение файла 3
                if(bUseCurrentEncoding)
                    m_sd3->readAndPreprocess(m_sd3->getEncoding(), false, m_pMyOptions);
                else
                    m_sd3->readAndPreprocess(options->m_pEncodingC, options->m_bAutoDetectUnicodeC, m_pMyOptions);

                pp.step();
            }

            pTotalDiffStatus->setBinaryEqualAB(m_sd1->isBinaryEqualWith(m_sd2));
            pTotalDiffStatus->setBinaryEqualAC(m_sd1->isBinaryEqualWith(m_sd3));
            pTotalDiffStatus->setBinaryEqualBC(m_sd3->isBinaryEqualWith(m_sd2));

            pp.setInformation(i18n("Diff: A <-> B"));
            qCInfo(kdiffMain) << i18n("Diff: A <-> B");

            // Дифф между 1 и 2
            if(m_sd1->isText() && m_sd2->isText())
            {
                m_manualDiffHelpList.runDiff(m_sd1->getLineDataForDiff(), m_sd1->getSizeLines(), m_sd2->getLineDataForDiff(), m_sd2->getSizeLines(), m_diffList12, e_SrcSelector::A, e_SrcSelector::B,
                                             options);

                m_diff3LineList.calcDiff3LineListUsingAB(&m_diffList12);
            }
            pp.step();
            m_diff3LineList.debugLineCheck(m_sd1->getSizeLines(), e_SrcSelector::A);

            pp.setInformation(i18n("Diff: A <-> C"));
            qCInfo(kdiffMain) << i18n("Diff: A <-> C");

            // Дифф между 1 и 3
            if(m_sd1->isText() && m_sd3->isText())
            {
                m_manualDiffHelpList.runDiff(m_sd1->getLineDataForDiff(), m_sd1->getSizeLines(), m_sd3->getLineDataForDiff(), m_sd3->getSizeLines(), m_diffList13, e_SrcSelector::A, e_SrcSelector::C,
                                             options);

                m_diff3LineList.calcDiff3LineListUsingAC(&m_diffList13);
                //m_diff3LineList.dump();
                m_diff3LineList.correctManualDiffAlignment(&m_manualDiffHelpList);
                //m_diff3LineList.dump();
                m_diff3LineList.calcDiff3LineListTrim(m_sd1->getLineDataForDiff(), m_sd2->getLineDataForDiff(), m_sd3->getLineDataForDiff(), &m_manualDiffHelpList);
            }
            pp.step();

            pp.setInformation(i18n("Diff: B <-> C"));
            qCInfo(kdiffMain) << i18n("Diff: B <-> C");

            // Дифф между 2 и 3
            if(m_sd2->isText() && m_sd3->isText())
            {
                m_manualDiffHelpList.runDiff(m_sd2->getLineDataForDiff(), m_sd2->getSizeLines(), m_sd3->getLineDataForDiff(), m_sd3->getSizeLines(), m_diffList23, e_SrcSelector::B, e_SrcSelector::C,
                                             options);
                if(options->m_bDiff3AlignBC)
                {
                    m_diff3LineList.calcDiff3LineListUsingBC(&m_diffList23);
                    m_diff3LineList.correctManualDiffAlignment(&m_manualDiffHelpList);
                    m_diff3LineList.calcDiff3LineListTrim(m_sd1->getLineDataForDiff(), m_sd2->getLineDataForDiff(), m_sd3->getLineDataForDiff(), &m_manualDiffHelpList);
                }
            }
            pp.step();

            m_diff3LineList.debugLineCheck(m_sd1->getSizeLines(), e_SrcSelector::A);
            m_diff3LineList.debugLineCheck(m_sd2->getSizeLines(), e_SrcSelector::B);
            m_diff3LineList.debugLineCheck(m_sd3->getSizeLines(), e_SrcSelector::C);

            pp.setInformation(i18n("Linediff: A <-> B"));
            qCInfo(kdiffMain) << i18n("Linediff: A <-> B");
            if(m_sd1->hasData() && m_sd2->hasData() && m_sd1->isText() && m_sd2->isText())
                pTotalDiffStatus->setTextEqualAB(m_diff3LineList.fineDiff(e_SrcSelector::A, m_sd1->getLineDataForDisplay(), m_sd2->getLineDataForDisplay(), eIgnoreFlags));
            pp.step();

            pp.setInformation(i18n("Linediff: B <-> C"));
            qCInfo(kdiffMain) << i18n("Linediff: B <-> C");
            if(m_sd2->hasData() && m_sd3->hasData() && m_sd2->isText() && m_sd3->isText())
                pTotalDiffStatus->setTextEqualBC(m_diff3LineList.fineDiff(e_SrcSelector::B, m_sd2->getLineDataForDisplay(), m_sd3->getLineDataForDisplay(), eIgnoreFlags));
            pp.step();

            pp.setInformation(i18n("Linediff: A <-> C"));
            qCInfo(kdiffMain) << i18n("Linediff: A <-> C");
            if(m_sd1->hasData() && m_sd3->hasData() && m_sd1->isText() && m_sd3->isText())
                pTotalDiffStatus->setTextEqualAC(m_diff3LineList.fineDiff(e_SrcSelector::C, m_sd3->getLineDataForDisplay(), m_sd1->getLineDataForDisplay(), eIgnoreFlags));
            m_diff3LineList.debugLineCheck(m_sd2->getSizeLines(), e_SrcSelector::B);
            m_diff3LineList.debugLineCheck(m_sd3->getSizeLines(), e_SrcSelector::C);

            pp.setInformation(i18n("Linediff: A <-> B"));
            if(m_sd1->hasData() && m_sd2->hasData() && m_sd1->isText() && m_sd2->isText())
                pTotalDiffStatus->setTextEqualAB(m_diff3LineList.fineDiff(e_SrcSelector::A, m_sd1->getLineDataForDisplay(), m_sd2->getLineDataForDisplay(), eIgnoreFlags));
            pp.step();

            pp.setInformation(i18n("Linediff: B <-> C"));
            if(m_sd3->hasData() && m_sd2->hasData() && m_sd3->isText() && m_sd2->isText())
                pTotalDiffStatus->setTextEqualBC(m_diff3LineList.fineDiff(e_SrcSelector::B, m_sd2->getLineDataForDisplay(), m_sd3->getLineDataForDisplay(), eIgnoreFlags));
            pp.step();

            pp.setInformation(i18n("Linediff: A <-> C"));
            if(m_sd1->hasData() && m_sd3->hasData() && m_sd1->isText() && m_sd3->isText())
                pTotalDiffStatus->setTextEqualAC(m_diff3LineList.fineDiff(e_SrcSelector::C, m_sd3->getLineDataForDisplay(), m_sd1->getLineDataForDisplay(), eIgnoreFlags));
            pp.step();
            if(m_sd1->getSizeBytes() == 0)
            {
                pTotalDiffStatus->setTextEqualAB(false);
                pTotalDiffStatus->setTextEqualAC(false);
            }
            if(m_sd2->getSizeBytes() == 0)
            {
                pTotalDiffStatus->setTextEqualAB(false);
                pTotalDiffStatus->setTextEqualBC(false);
            }
        }
        errors.append(m_sd3->getErrors());
    }
    else
    {
        pp.clear();
    }

    if(errors.isEmpty() && m_sd1->isText() && m_sd2->isText())
    {
        m_diffBufferInfo->init(&m_diff3LineList, &m_diff3LineVector,
                                               m_sd1->getLineDataForDiff(), m_sd1->getSizeLines(),
                                               m_sd2->getLineDataForDiff(), m_sd2->getSizeLines(),
                                               m_sd3->getLineDataForDiff(), m_sd3->getSizeLines());
        Diff3Line::m_pDiffBufferInfo = m_diffBufferInfo;

        m_diff3LineList.calcWhiteDiff3Lines(m_sd1->getLineDataForDiff(), m_sd2->getLineDataForDiff(), m_sd3->getLineDataForDiff(), options->ignoreComments());
        m_diff3LineList.calcDiff3LineVector(m_diff3LineVector);
    }
}