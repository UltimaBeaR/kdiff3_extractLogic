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

bool MergeDataObj::getOutFileData(QTextCodec* pEncoding, e_LineEndStyle eLineEndStyle, void*& out_pBuffer, qint64& out_bufferLength)
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

            LineRef srcLine = mel.src() == e_SrcSelector::A ? mel.id3l()->getLineA() : mel.src() == e_SrcSelector::B ? mel.id3l()->getLineB()
                                                                                   : mel.src() == e_SrcSelector::C   ? mel.id3l()->getLineC()
                                                                                                                     : LineRef();

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

void MergeDataObj::merge(bool bAutoSolve, e_SrcSelector defaultSelector, bool bConflictsOnly, bool bWhiteSpaceOnly, std::function<void()> callback1, std::function<void()> callback2)
{
    if(!bConflictsOnly)
    {
        m_mergeLineList.clear();

        int lineIdx = 0;
        Diff3LineList::const_iterator it;
        for(it = m_pDiff3LineList->begin(); it != m_pDiff3LineList->end(); ++it, ++lineIdx)
        {
            const Diff3Line& d = *it;

            MergeLine ml;
            bool bLineRemoved;
            d.mergeOneLine(ml.mergeDetails, ml.bConflict, bLineRemoved, ml.srcSelect, m_pldC == nullptr);

            // Automatic solving for only whitespace changes.
            if(ml.bConflict &&
               ((m_pldC == nullptr && (d.isEqualAB() || (d.isWhiteLine(e_SrcSelector::A) && d.isWhiteLine(e_SrcSelector::B)))) ||
                (m_pldC != nullptr && ((d.isEqualAB() && d.isEqualAC()) || (d.isWhiteLine(e_SrcSelector::A) && d.isWhiteLine(e_SrcSelector::B) && d.isWhiteLine(e_SrcSelector::C))))))
            {
                ml.bWhiteSpaceConflict = true;
            }

            ml.d3lLineIdx = lineIdx;
            ml.bDelta = ml.srcSelect != e_SrcSelector::A;
            ml.id3l = it;
            ml.srcRangeLength = 1;

            MergeLine* back = m_mergeLineList.empty() ? nullptr : &m_mergeLineList.back();

            bool bSame = back != nullptr && sameKindCheck(ml, *back);
            if(bSame)
            {
                ++back->srcRangeLength;
                if(back->bWhiteSpaceConflict && !ml.bWhiteSpaceConflict)
                    back->bWhiteSpaceConflict = false;
            }
            else
            {
                m_mergeLineList.push_back(ml);
            }

            if(!ml.bConflict)
            {
                MergeLine& tmpBack = m_mergeLineList.back();
                MergeEditLine mel(ml.id3l);
                mel.setSource(ml.srcSelect, bLineRemoved);
                tmpBack.mergeEditLineList.push_back(mel);
            }
            else if(back == nullptr || !back->bConflict || !bSame)
            {
                MergeLine& tmpBack = m_mergeLineList.back();
                MergeEditLine mel(ml.id3l);
                mel.setConflict();
                tmpBack.mergeEditLineList.push_back(mel);
            }
        }
    }

    bool bSolveWhiteSpaceConflicts = false;
    if(bAutoSolve) // when true, then the other params are not used and we can change them here. (see all invocations of merge())
    {
        if(m_pldC == nullptr && m_pMyOptions->m_whiteSpace2FileMergeDefault != (int)e_SrcSelector::None) // Only two inputs
        {
            Q_ASSERT(m_pMyOptions->m_whiteSpace2FileMergeDefault <= (int)e_SrcSelector::Max && m_pMyOptions->m_whiteSpace2FileMergeDefault >= (int)e_SrcSelector::Min);
            defaultSelector = (e_SrcSelector)m_pMyOptions->m_whiteSpace2FileMergeDefault;
            bWhiteSpaceOnly = true;
            bSolveWhiteSpaceConflicts = true;
        }
        else if(m_pldC != nullptr && m_pMyOptions->m_whiteSpace3FileMergeDefault != (int)e_SrcSelector::None)
        {
            Q_ASSERT(m_pMyOptions->m_whiteSpace3FileMergeDefault <= (int)e_SrcSelector::Max && m_pMyOptions->m_whiteSpace2FileMergeDefault >= (int)e_SrcSelector::Min);
            defaultSelector = (e_SrcSelector)m_pMyOptions->m_whiteSpace3FileMergeDefault;
            bWhiteSpaceOnly = true;
            bSolveWhiteSpaceConflicts = true;
        }
    }

    if(!bAutoSolve || bSolveWhiteSpaceConflicts)
    {
        // Change all auto selections
        MergeLineList::iterator mlIt;
        for(mlIt = m_mergeLineList.begin(); mlIt != m_mergeLineList.end(); ++mlIt)
        {
            MergeLine& ml = *mlIt;
            bool bConflict = ml.mergeEditLineList.empty() || ml.mergeEditLineList.begin()->isConflict();
            if(ml.bDelta && (!bConflictsOnly || bConflict) && (!bWhiteSpaceOnly || ml.bWhiteSpaceConflict))
            {
                ml.mergeEditLineList.clear();
                if(defaultSelector == e_SrcSelector::Invalid && ml.bDelta)
                {
                    MergeEditLine mel(ml.id3l);

                    mel.setConflict();
                    ml.bConflict = true;
                    ml.mergeEditLineList.push_back(mel);
                }
                else
                {
                    Diff3LineList::const_iterator d3llit = ml.id3l;
                    int j;

                    for(j = 0; j < ml.srcRangeLength; ++j)
                    {
                        MergeEditLine mel(d3llit);
                        mel.setSource(defaultSelector, false);

                        LineRef srcLine = defaultSelector == e_SrcSelector::A ? d3llit->getLineA() : defaultSelector == e_SrcSelector::B ? d3llit->getLineB()
                                                                                                 : defaultSelector == e_SrcSelector::C   ? d3llit->getLineC()
                                                                                                                                         : LineRef();

                        if(srcLine.isValid())
                        {
                            ml.mergeEditLineList.push_back(mel);
                        }

                        ++d3llit;
                    }

                    if(ml.mergeEditLineList.empty()) // Make a line nevertheless
                    {
                        MergeEditLine mel(ml.id3l);
                        mel.setRemoved(defaultSelector);
                        ml.mergeEditLineList.push_back(mel);
                    }
                }
            }
        }
    }

    MergeLineList::iterator mlIt;
    for(mlIt = m_mergeLineList.begin(); mlIt != m_mergeLineList.end(); ++mlIt)
    {
        MergeLine& ml = *mlIt;
        // Remove all lines that are empty, because no src lines are there.

        LineRef oldSrcLine;
        e_SrcSelector oldSrc = e_SrcSelector::Invalid;
        MergeEditLineList::iterator melIt;
        for(melIt = ml.mergeEditLineList.begin(); melIt != ml.mergeEditLineList.end();)
        {
            MergeEditLine& mel = *melIt;
            e_SrcSelector melsrc = mel.src();

            LineRef srcLine = mel.isRemoved() ? LineRef() : melsrc == e_SrcSelector::A ? mel.id3l()->getLineA()
                                                        : melsrc == e_SrcSelector::B   ? mel.id3l()->getLineB()
                                                        : melsrc == e_SrcSelector::C   ? mel.id3l()->getLineC()
                                                                                       : LineRef();

            // At least one line remains because oldSrc != melsrc for first line in list
            // Other empty lines will be removed
            if(!srcLine.isValid() && !oldSrcLine.isValid() && oldSrc == melsrc)
                melIt = ml.mergeEditLineList.erase(melIt);
            else
                ++melIt;

            oldSrcLine = srcLine;
            oldSrc = melsrc;
        }
    }

    if(bAutoSolve && !bConflictsOnly)
    {
        callback1();
    }

    int nrOfSolvedConflicts = 0;
    int nrOfUnsolvedConflicts = 0;
    int nrOfWhiteSpaceConflicts = 0;

    MergeLineList::iterator i;
    for(i = m_mergeLineList.begin(); i != m_mergeLineList.end(); ++i)
    {
        if(i->bConflict)
            ++nrOfUnsolvedConflicts;
        else if(i->bDelta)
            ++nrOfSolvedConflicts;

        if(i->bWhiteSpaceConflict)
            ++nrOfWhiteSpaceConflicts;
    }

    m_pTotalDiffStatus->setUnsolvedConflicts(nrOfUnsolvedConflicts);
    m_pTotalDiffStatus->setSolvedConflicts(nrOfSolvedConflicts);
    m_pTotalDiffStatus->setWhitespaceConflicts(nrOfWhiteSpaceConflicts);

    callback2();

    m_currentMergeLineIt = m_mergeLineList.begin();
}

bool MergeDataObj::sameKindCheck(const MergeLine& ml1, const MergeLine& ml2)
{
    if(ml1.bConflict && ml2.bConflict)
    {
        // Both lines have conflicts: If one is only a white space conflict and
        // the other one is a real conflict, then this line returns false.
        return ml1.id3l->isEqualAC() == ml2.id3l->isEqualAC() && ml1.id3l->isEqualAB() == ml2.id3l->isEqualAB();
    }
    else
        return (
            (!ml1.bConflict && !ml2.bConflict && ml1.bDelta && ml2.bDelta && ml1.srcSelect == ml2.srcSelect && (ml1.mergeDetails == ml2.mergeDetails || (ml1.mergeDetails != e_MergeDetails::eBCAddedAndEqual && ml2.mergeDetails != e_MergeDetails::eBCAddedAndEqual))) ||
            (!ml1.bDelta && !ml2.bDelta));
}