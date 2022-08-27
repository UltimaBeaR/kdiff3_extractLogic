#include "EXTRACT/my/MergeDataObj.h"

// Эти инклюды взял с окошка (не факт что они все нужны) просто чтобы потом не париться при реализации.
// Потом в итоге нужно будет поубирать ненужное (сейчас убирать не надо, даже если оно не используется - в процессе рефакторинга что то может начать использоваться)
// --------------------------------------------------------------------

#include "EXTRACT/0_consider/ui/KDiff3App_kdiff3.h"
#include "EXTRACT/2_final/defmac.h"
#include "EXTRACT/2_final/options.h"
#include "EXTRACT/2_final/utils/Utils.h" // for Utils
#include "RLPainter.h"
#include "guiutils.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QCursor>
#include <QFile>
#include <QHBoxLayout>
#include <QPainter>
#include <QPixmap>
#include <QPointer>
#include <QRegExp>
#include <QResizeEvent>
#include <QStatusBar>
#include <QTextCodec>
#include <QTextLayout>
#include <QTextStream>
#include <QTimerEvent>
#include <QtMath>

#include <KActionCollection>
#include <KLocalizedString>
#include <KMessageBox>

// --------------------------------------------------------------------

MergeDataObj::MergeDataObj()
{
}

void MergeDataObj::init_setOptions(const MyOptions* pMyOptions)
{
    m_pMyOptions = pMyOptions;
}

void MergeDataObj::init(
    const QVector<LineData>* pLineDataA, LineRef sizeA,
    const QVector<LineData>* pLineDataB, LineRef sizeB,
    const QVector<LineData>* pLineDataC, LineRef sizeC,
    const Diff3LineList* pDiff3LineList,
    TotalDiffStatus* pTotalDiffStatus)
{
    m_pldA = pLineDataA;
    m_pldB = pLineDataB;
    m_pldC = pLineDataC;
    m_sizeA = sizeA;
    m_sizeB = sizeB;
    m_sizeC = sizeC;

    m_pDiff3LineList = pDiff3LineList;
    m_pTotalDiffStatus = pTotalDiffStatus;
}

void MergeDataObj::reset()
{
    m_pDiff3LineList = nullptr;
    m_pTotalDiffStatus = nullptr;
    m_pldA = nullptr;
    m_pldB = nullptr;
    m_pldC = nullptr;
}

bool MergeDataObj::getOutFileData(QTextCodec* pEncoding, e_LineEndStyle eLineEndStyle, void* &out_pBuffer, qint64 &out_bufferLength)
{
    if(getNrOfUnsolvedConflicts() > 0)
    {
        // Эта же логика повторяется в MergeResultWindow::saveDocument перед вызовом этого метода и там идет алерт в этом случае
        return false;
    }

    if(eLineEndStyle == eLineEndStyleConflict || eLineEndStyle == eLineEndStyleUndefined)
    {
        // Эта же логика повторяется в MergeResultWindow::saveDocument перед вызовом этого метода и там идет алерт в этом случае
        return false;
    }

    QByteArray dataArray;
    QTextStream textOutStream(&dataArray, QIODevice::WriteOnly);
    if(pEncoding->name() == "UTF-8")
        textOutStream.setGenerateByteOrderMark(false); // Shouldn't be necessary. Bug in Qt or docs
    else
        textOutStream.setGenerateByteOrderMark(true); // Only for UTF-16
    textOutStream.setCodec(pEncoding);

    // Determine the line feed for this file
    const QString lineFeed(eLineEndStyle == eLineEndStyleDos ? QString("\r\n") : QString("\n"));

    int line = 0;
    MergeLineList::iterator mlIt = m_mergeLineList.begin();
    for(mlIt = m_mergeLineList.begin(); mlIt != m_mergeLineList.end(); ++mlIt)
    {
        MergeLine& ml = *mlIt;
        MergeEditLineList::iterator melIt;
        for(melIt = ml.mergeEditLineList.begin(); melIt != ml.mergeEditLineList.end(); ++melIt)
        {
            MergeEditLine& mel = *melIt;

            if(mel.isEditableText())
            {
                const QString str = mel.getString(m_pldA, m_pldB, m_pldC);

                if(line > 0 && !mel.isRemoved())
                {
                    // Put line feed between lines, but not for the first line
                    // or between lines that have been removed (because there
                    // isn't a line there).
                    textOutStream << lineFeed;
                }

                textOutStream << str;
                ++line;
            }
        }
    }
    textOutStream.flush();

    out_pBuffer = dataArray.data();
    out_bufferLength = dataArray.size();

    return true;
}

int MergeDataObj::getNrOfUnsolvedConflicts(int* pNrOfWhiteSpaceConflicts)
{
    int nrOfUnsolvedConflicts = 0;
    if(pNrOfWhiteSpaceConflicts != nullptr)
        *pNrOfWhiteSpaceConflicts = 0;

    MergeLineList::iterator mlIt = m_mergeLineList.begin();
    for(mlIt = m_mergeLineList.begin(); mlIt != m_mergeLineList.end(); ++mlIt)
    {
        MergeLine& ml = *mlIt;
        MergeEditLineList::iterator melIt = ml.mergeEditLineList.begin();
        if(melIt->isConflict())
        {
            ++nrOfUnsolvedConflicts;
            if(ml.bWhiteSpaceConflict && pNrOfWhiteSpaceConflicts != nullptr)
                ++*pNrOfWhiteSpaceConflicts;
        }
    }

    return nrOfUnsolvedConflicts;
}

void MergeDataObj::choose(e_SrcSelector selector)
{
    // Эту проверку повторяю, т.к. в окошке она есть, но между проверкой и основным кодом есть ui-ная штука, по этому тут дублирую (на случай вызова не из того метода, чтобы стейт был валидным)
    if(m_currentMergeLineIt == m_mergeLineList.end())
        return;



    // First find range for which this change works.
    MergeLine& ml = *m_currentMergeLineIt;

    MergeEditLineList::iterator melIt;

    // Now check if selector is active for this range already.
    bool bActive = false;

    // Remove unneeded lines in the range.
    for(melIt = ml.mergeEditLineList.begin(); melIt != ml.mergeEditLineList.end();)
    {
        MergeEditLine& mel = *melIt;
        if(mel.src() == selector)
            bActive = true;

        if(mel.src() == selector || !mel.isEditableText() || mel.isModified())
            melIt = ml.mergeEditLineList.erase(melIt);
        else
            ++melIt;
    }

    if(!bActive) // Selected source wasn't active.
    {            // Append the lines from selected source here at rangeEnd.
        Diff3LineList::const_iterator d3llit = ml.id3l;
        int j;

        for(j = 0; j < ml.srcRangeLength; ++j)
        {
            MergeEditLine mel(d3llit);
            mel.setSource(selector, false);
            ml.mergeEditLineList.push_back(mel);

            ++d3llit;
        }
    }

    if(!ml.mergeEditLineList.empty())
    {
        // Remove all lines that are empty, because no src lines are there.
        for(melIt = ml.mergeEditLineList.begin(); melIt != ml.mergeEditLineList.end();)
        {
            MergeEditLine& mel = *melIt;

            LineRef srcLine = mel.src() == e_SrcSelector::A ? mel.id3l()->getLineA() : mel.src() == e_SrcSelector::B ? mel.id3l()->getLineB() : mel.src() == e_SrcSelector::C ? mel.id3l()->getLineC() : LineRef();

            if(!srcLine.isValid())
                melIt = ml.mergeEditLineList.erase(melIt);
            else
                ++melIt;
        }
    }

    if(ml.mergeEditLineList.empty())
    {
        // Insert a dummy line:
        MergeEditLine mel(ml.id3l);

        if(bActive)
            mel.setConflict(); // All src entries deleted => conflict
        else
            mel.setRemoved(selector); // No lines in corresponding src found.

        ml.mergeEditLineList.push_back(mel);
    }
}