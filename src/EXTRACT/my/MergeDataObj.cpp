#include "EXTRACT/my/MergeDataObj.h"

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