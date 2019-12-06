/***************************************************************************
 *   Copyright (C) 2003-2007 by Joachim Eibl                               *
 *   joachim.eibl at gmx.de                                                *
 *   Copyright (C) 2018 Michael Reeves reeves.87@gmail.com                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "difftextwindow.h"

#include "FileNameLineEdit.h"
#include "RLPainter.h"
#include "SourceData.h" // for SourceData
#include "Utils.h"      // for Utils
#include "common.h"     // for getAtomic, max3, min3
#include "kdiff3.h"
#include "merger.h"
#include "options.h"
#include "progress.h"
#include "selection.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include <KLocalizedString>

#include <QDir>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QPushButton>
#include <QRunnable>
#include <QStatusBar>
#include <QTextCodec>
#include <QTextLayout>
#include <QThreadPool>
#include <QToolTip>
#include <QUrl>
#include <QtMath>

QList<RecalcWordWrapRunnable*> DiffTextWindow::s_runnables; //Used in startRunables and recalWordWrap

class RecalcWordWrapRunnable : public QRunnable
{
  private:
    static QAtomicInt s_runnableCount;
    DiffTextWindow* m_pDTW;
    int m_visibleTextWidth;
    int m_cacheIdx;

  public:
    static QAtomicInt s_maxNofRunnables;
    RecalcWordWrapRunnable(DiffTextWindow* p, int visibleTextWidth, int cacheIdx)
        : m_pDTW(p), m_visibleTextWidth(visibleTextWidth), m_cacheIdx(cacheIdx)
    {
        setAutoDelete(true);
        s_runnableCount.fetchAndAddOrdered(1);
    }
    void run() override
    {
        m_pDTW->recalcWordWrapHelper(0, m_visibleTextWidth, m_cacheIdx);
        int newValue = s_runnableCount.fetchAndAddOrdered(-1) - 1;
        g_pProgressDialog->setCurrent(s_maxNofRunnables - getAtomic(s_runnableCount));
        if(newValue == 0)
        {
            emit m_pDTW->finishRecalcWordWrap(m_visibleTextWidth);
        }
    }
};

QAtomicInt RecalcWordWrapRunnable::s_runnableCount = 0;
QAtomicInt RecalcWordWrapRunnable::s_maxNofRunnables = 0;

class WrapLineCacheData
{
  public:
    WrapLineCacheData() {}
    WrapLineCacheData(int d3LineIdx, int textStart, int textLength)
        : m_d3LineIdx(d3LineIdx), m_textStart(textStart), m_textLength(textLength) {}
    qint32 d3LineIdx() { return m_d3LineIdx; }
    qint32 textStart() { return m_textStart; }
    qint32 textLength() { return m_textLength; }

  private:
    qint32 m_d3LineIdx = 0;
    qint32 m_textStart = 0;
    qint32 m_textLength = 0;
};

class DiffTextWindowData
{
  public:
    explicit DiffTextWindowData(DiffTextWindow* p)
    {
        m_pDiffTextWindow = p;
#if defined(Q_OS_WIN)
        m_eLineEndStyle = eLineEndStyleDos;
#else
        m_eLineEndStyle = eLineEndStyleUnix;
#endif
    }

    QString getString(int d3lIdx);
    QString getLineString(int line);

    void writeLine(
        RLPainter& p, const LineData* pld,
        const DiffList* pLineDiff1, const DiffList* pLineDiff2, const LineRef& line,
        const ChangeFlags whatChanged, const ChangeFlags whatChanged2, const LineRef& srcLineIdx,
        int wrapLineOffset, int wrapLineLength, bool bWrapLine, const QRect& invalidRect, int deviceWidth);

    void draw(RLPainter& p, const QRect& invalidRect, int deviceWidth, int beginLine, int endLine);

    void myUpdate(int afterMilliSecs);

    int leftInfoWidth() { return 4 + m_lineNumberWidth; } // Nr of information columns on left side
    int convertLineOnScreenToLineInSource(int lineOnScreen, e_CoordType coordType, bool bFirstLine);

    void prepareTextLayout(QTextLayout& textLayout, bool bFirstLine, int visibleTextWidth = -1);

    bool isThreeWay() const { return m_bTriple; };
    const QString& getFileName() { return m_filename; }

    const Diff3LineVector* getDiff3LineVector() { return m_pDiff3LineVector; }

  private:
    //TODO: Remove friend classes after creating accessors. Please don't add new classes here
    friend DiffTextWindow;
    DiffTextWindow* m_pDiffTextWindow;
    DiffTextWindowFrame* m_pDiffTextWindowFrame = nullptr;
    QTextCodec* m_pTextCodec = nullptr;
    e_LineEndStyle m_eLineEndStyle;

    const QVector<LineData>* m_pLineData = nullptr;
    int m_size = 0;
    QString m_filename;
    bool m_bWordWrap = false;
    int m_delayedDrawTimer = 0;

    const Diff3LineVector* m_pDiff3LineVector = nullptr;
    Diff3WrapLineVector m_diff3WrapLineVector;
    const ManualDiffHelpList* m_pManualDiffHelpList = nullptr;
    QList<QVector<WrapLineCacheData>> m_wrapLineCacheList;

    QSharedPointer<Options> m_pOptions;
    QColor m_cThis;
    QColor m_cDiff1;
    QColor m_cDiff2;
    QColor m_cDiffBoth;

    int m_fastSelectorLine1 = 0;
    int m_fastSelectorNofLines = 0;

    bool m_bTriple = false;
    e_SrcSelector m_winIdx = None;
    int m_firstLine = 0;
    int m_oldFirstLine = 0;
    int m_horizScrollOffset = 0;
    int m_lineNumberWidth = 0;
    QAtomicInt m_maxTextWidth = -1;

    QStatusBar* m_pStatusBar = nullptr;

    Selection m_selection;

    int m_scrollDeltaX = 0;
    int m_scrollDeltaY = 0;

    bool m_bMyUpdate = false;

    bool m_bSelectionInProgress = false;
    QPoint m_lastKnownMousePos;
};

bool DiffTextWindow::isThreeWay() const { return d->isThreeWay(); };
const QString& DiffTextWindow::getFileName() const { return d->getFileName(); }

e_SrcSelector DiffTextWindow::getWindowIndex() const { return d->m_winIdx; };

const QString DiffTextWindow::getEncodingDisplayString() const { return d->m_pTextCodec != nullptr ? QLatin1String(d->m_pTextCodec->name()) : QString(); }
e_LineEndStyle DiffTextWindow::getLineEndStyle() const { return d->m_eLineEndStyle; }

const Diff3LineVector* DiffTextWindow::getDiff3LineVector() const { return d->getDiff3LineVector(); }

qint32 DiffTextWindow::getLineNumberWidth() const { return (int)log10((double)std::max(d->m_size, 1)) + 1; }

DiffTextWindow::DiffTextWindow(
    DiffTextWindowFrame* pParent,
    QStatusBar* pStatusBar,
    const QSharedPointer<Options>& pOptions,
    e_SrcSelector winIdx)
    : QWidget(pParent)
{
    setObjectName(QString("DiffTextWindow%1").arg(winIdx));
    setAttribute(Qt::WA_OpaquePaintEvent);
    //setAttribute( Qt::WA_PaintOnScreen );
    setUpdatesEnabled(false);

    d = new DiffTextWindowData(this);
    d->m_pDiffTextWindowFrame = pParent;
    setFocusPolicy(Qt::ClickFocus);
    setAcceptDrops(true);

    d->m_pOptions = pOptions;
    init(QString(""), nullptr, d->m_eLineEndStyle, nullptr, 0, nullptr, nullptr, false);

    setMinimumSize(QSize(20, 20));

    d->m_pStatusBar = pStatusBar;
    setUpdatesEnabled(true);
    d->m_bWordWrap = false;
    d->m_winIdx = winIdx;

    setFont(d->m_pOptions->m_font);
}

DiffTextWindow::~DiffTextWindow()
{
    delete d;
}

void DiffTextWindow::init(
    const QString& filename,
    QTextCodec* pTextCodec,
    e_LineEndStyle eLineEndStyle,
    const QVector<LineData>* pLineData,
    int size,
    const Diff3LineVector* pDiff3LineVector,
    const ManualDiffHelpList* pManualDiffHelpList,
    bool bTriple)
{
    d->m_filename = filename;
    d->m_pLineData = pLineData;
    d->m_size = size;
    d->m_pDiff3LineVector = pDiff3LineVector;
    d->m_diff3WrapLineVector.clear();
    d->m_pManualDiffHelpList = pManualDiffHelpList;

    d->m_firstLine = 0;
    d->m_oldFirstLine = -1;
    d->m_horizScrollOffset = 0;
    d->m_bTriple = bTriple;
    d->m_scrollDeltaX = 0;
    d->m_scrollDeltaY = 0;
    d->m_bMyUpdate = false;
    d->m_fastSelectorLine1 = 0;
    d->m_fastSelectorNofLines = 0;
    d->m_lineNumberWidth = 0;
    d->m_maxTextWidth = -1;

    d->m_pTextCodec = pTextCodec;
    d->m_eLineEndStyle = eLineEndStyle;

    update();
    d->m_pDiffTextWindowFrame->init();
}

void DiffTextWindow::reset()
{
    d->m_pLineData = nullptr;
    d->m_size = 0;
    d->m_pDiff3LineVector = nullptr;
    d->m_filename = "";
    d->m_diff3WrapLineVector.clear();
}

void DiffTextWindow::setPaintingAllowed(bool bAllowPainting)
{
    if(updatesEnabled() != bAllowPainting)
    {
        setUpdatesEnabled(bAllowPainting);
        if(bAllowPainting)
            update();
        else
            reset();
    }
}

void DiffTextWindow::dragEnterEvent(QDragEnterEvent* e)
{
    e->setAccepted(e->mimeData()->hasUrls() || e->mimeData()->hasText());
    // TODO: Move this to DiffTextWindow::dropEvent
    // Note that the corresponding drop is handled in KDiff3App::eventFilter().
}

void DiffTextWindow::printWindow(RLPainter& painter, const QRect& view, const QString& headerText, int line, int linesPerPage, const QColor& fgColor)
{
    QRect clipRect = view;
    clipRect.setTop(0);
    painter.setClipRect(clipRect);
    painter.translate(view.left(), 0);
    QFontMetrics fm = painter.fontMetrics();
    //if ( fm.width(headerText) > view.width() )
    {
        // A simple wrapline algorithm
        int l = 0;
        for(int p = 0; p < headerText.length();)
        {
            QString s = headerText.mid(p);
            int i;
            for(i = 2; i < s.length(); ++i)
                if(Utils::getHorizontalAdvance(fm, s, i) > view.width())
                {
                    --i;
                    break;
                }
            //QString s2 = s.left(i);
            painter.drawText(0, l * fm.height() + fm.ascent(), s.left(i));
            p += i;
            ++l;
        }
        painter.setPen(fgColor);
        painter.drawLine(0, view.top() - 2, view.width(), view.top() - 2);
    }

    painter.translate(0, view.top());
    print(painter, view, line, linesPerPage);
    painter.resetTransform();
}

void DiffTextWindow::setFirstLine(QtNumberType firstLine)
{
    int fontHeight = fontMetrics().lineSpacing();

    LineRef newFirstLine = std::max(0, firstLine);

    int deltaY = fontHeight * (d->m_firstLine - newFirstLine);

    d->m_firstLine = newFirstLine;

    if(d->m_bSelectionInProgress && d->m_selection.isValidFirstLine())
    {
        LineRef line;
        int pos;
        convertToLinePos(d->m_lastKnownMousePos.x(), d->m_lastKnownMousePos.y(), line, pos);
        d->m_selection.end(line, pos);
        update();
    }
    else
    {
        scroll(0, deltaY);
    }
    d->m_pDiffTextWindowFrame->setFirstLine(d->m_firstLine);
}

int DiffTextWindow::getFirstLine()
{
    return d->m_firstLine;
}

void DiffTextWindow::setHorizScrollOffset(int horizScrollOffset)
{
    int fontWidth = Utils::getHorizontalAdvance(fontMetrics(), '0');
    int xOffset = d->leftInfoWidth() * fontWidth;

    int deltaX = d->m_horizScrollOffset - std::max(0, horizScrollOffset);

    d->m_horizScrollOffset = std::max(0, horizScrollOffset);

    QRect r(xOffset, 0, width() - xOffset, height());

    if(d->m_pOptions->m_bRightToLeftLanguage)
    {
        deltaX = -deltaX;
        r = QRect(width() - xOffset - 2, 0, -(width() - xOffset), height()).normalized();
    }

    if(d->m_bSelectionInProgress && d->m_selection.isValidFirstLine())
    {
        LineRef line;
        int pos;
        convertToLinePos(d->m_lastKnownMousePos.x(), d->m_lastKnownMousePos.y(), line, pos);
        d->m_selection.end(line, pos);
        update();
    }
    else
    {
        scroll(deltaX, 0, r);
    }
}

int DiffTextWindow::getMaxTextWidth()
{
    if(d->m_bWordWrap)
    {
        return getVisibleTextAreaWidth();
    }
    else if(getAtomic(d->m_maxTextWidth) < 0)
    {
        d->m_maxTextWidth = 0;
        QTextLayout textLayout(QString(), font(), this);
        for(int i = 0; i < d->m_size; ++i)
        {
            textLayout.clearLayout();
            textLayout.setText(d->getString(i));
            d->prepareTextLayout(textLayout, true);
            if(textLayout.maximumWidth() > getAtomic(d->m_maxTextWidth))
                d->m_maxTextWidth = qCeil(textLayout.maximumWidth());
        }
    }
    return getAtomic(d->m_maxTextWidth);
}

LineCount DiffTextWindow::getNofLines()
{
    return d->m_bWordWrap ? d->m_diff3WrapLineVector.size() : d->m_pDiff3LineVector->size();
}

int DiffTextWindow::convertLineToDiff3LineIdx(LineRef line)
{
    if(line.isValid() && d->m_bWordWrap && d->m_diff3WrapLineVector.size() > 0)
        return d->m_diff3WrapLineVector[std::min((LineRef::LineType)line, d->m_diff3WrapLineVector.size() - 1)].diff3LineIndex;
    else
        return line;
}

LineRef DiffTextWindow::convertDiff3LineIdxToLine(int d3lIdx)
{
    if(d->m_bWordWrap && d->m_pDiff3LineVector != nullptr && d->m_pDiff3LineVector->size() > 0)
        return (*d->m_pDiff3LineVector)[std::min(d3lIdx, (int)d->m_pDiff3LineVector->size() - 1)]->sumLinesNeededForDisplay;
    else
        return d3lIdx;
}

/** Returns a line number where the linerange [line, line+nofLines] can
    be displayed best. If it fits into the currently visible range then
    the returned value is the current firstLine.
*/
int getBestFirstLine(int line, int nofLines, int firstLine, int visibleLines)
{
    int newFirstLine = firstLine;
    if(line < firstLine || line + nofLines + 2 > firstLine + visibleLines)
    {
        if(nofLines > visibleLines || nofLines <= (2 * visibleLines / 3 - 1))
            newFirstLine = line - visibleLines / 3;
        else
            newFirstLine = line - (visibleLines - nofLines);
    }

    return newFirstLine;
}

void DiffTextWindow::setFastSelectorRange(int line1, int nofLines)
{
    d->m_fastSelectorLine1 = line1;
    d->m_fastSelectorNofLines = nofLines;
    if(isVisible())
    {
        int newFirstLine = getBestFirstLine(
            convertDiff3LineIdxToLine(d->m_fastSelectorLine1),
            convertDiff3LineIdxToLine(d->m_fastSelectorLine1 + d->m_fastSelectorNofLines) - convertDiff3LineIdxToLine(d->m_fastSelectorLine1),
            d->m_firstLine,
            getNofVisibleLines());
        if(newFirstLine != d->m_firstLine)
        {
            emit scrollDiffTextWindow(0, newFirstLine - d->m_firstLine);
        }

        update();
    }
}
/*
    Takes the line number estimated from mouse position and converts it to the actual line in
    the file. Then sets the status message accordingly.

    emits lineClicked signal.
*/
void DiffTextWindow::showStatusLine(const LineRef aproxLine)
{
    int d3lIdx = convertLineToDiff3LineIdx(aproxLine);

    if(d->m_pDiff3LineVector != nullptr && d3lIdx >= 0 && d3lIdx < (int)d->m_pDiff3LineVector->size())
    {
        const Diff3Line* pD3l = (*d->m_pDiff3LineVector)[d3lIdx];
        if(pD3l != nullptr)
        {
            LineRef actualLine = pD3l->getLineInFile(d->m_winIdx);

            QString message;
            if(actualLine.isValid())
                message = i18n("File %1: Line %2", d->m_filename, actualLine + 1);
            else
                message = i18n("File %1: Line not available", d->m_filename);
            if(d->m_pStatusBar != nullptr) d->m_pStatusBar->showMessage(message);

            emit lineClicked(d->m_winIdx, actualLine);
        }
    }
}

void DiffTextWindow::focusInEvent(QFocusEvent* e)
{
    emit gotFocus();
    QWidget::focusInEvent(e);
}

void DiffTextWindow::mousePressEvent(QMouseEvent* e)
{
    if(e->button() == Qt::LeftButton)
    {
        LineRef line;
        int pos;
        convertToLinePos(e->x(), e->y(), line, pos);

        int fontWidth = Utils::getHorizontalAdvance(fontMetrics(), '0');
        int xOffset = d->leftInfoWidth() * fontWidth;

        if((!d->m_pOptions->m_bRightToLeftLanguage && e->x() < xOffset) || (d->m_pOptions->m_bRightToLeftLanguage && e->x() > width() - xOffset))
        {
            emit setFastSelectorLine(convertLineToDiff3LineIdx(line));
            d->m_selection.reset(); // Disable current d->m_selection
        }
        else
        { // Selection
            resetSelection();
            d->m_selection.start(line, pos);
            d->m_selection.end(line, pos);
            d->m_bSelectionInProgress = true;
            d->m_lastKnownMousePos = e->pos();

            showStatusLine(line);
        }
    }
}

void DiffTextWindow::mouseDoubleClickEvent(QMouseEvent* e)
{
    d->m_bSelectionInProgress = false;
    d->m_lastKnownMousePos = e->pos();
    if(e->button() == Qt::LeftButton)
    {
        LineRef line;
        int pos;
        convertToLinePos(e->x(), e->y(), line, pos);

        // Get the string data of the current line
        QString s;
        if(d->m_bWordWrap)
        {
            if(line < 0 || line >= (int)d->m_diff3WrapLineVector.size())
                return;
            const Diff3WrapLine& d3wl = d->m_diff3WrapLineVector[line];
            s = d->getString(d3wl.diff3LineIndex).mid(d3wl.wrapLineOffset, d3wl.wrapLineLength);
        }
        else
        {
            if(line < 0 || line >= (int)d->m_pDiff3LineVector->size())
                return;
            s = d->getString(line);
        }

        if(!s.isEmpty())
        {
            int pos1, pos2;
            Utils::calcTokenPos(s, pos, pos1, pos2);

            resetSelection();
            d->m_selection.start(line, pos1);
            d->m_selection.end(line, pos2);
            update();
            // emit d->m_selectionEnd() happens in the mouseReleaseEvent.
            showStatusLine(line);
        }
    }
}

void DiffTextWindow::mouseReleaseEvent(QMouseEvent* e)
{
    d->m_bSelectionInProgress = false;
    d->m_lastKnownMousePos = e->pos();
    //if ( e->button() == LeftButton )
    {
        if(d->m_delayedDrawTimer)
            killTimer(d->m_delayedDrawTimer);
        d->m_delayedDrawTimer = 0;
        if(d->m_selection.isValidFirstLine())
        {
            emit selectionEnd();
        }
    }
    d->m_scrollDeltaX = 0;
    d->m_scrollDeltaY = 0;
}

void DiffTextWindow::mouseMoveEvent(QMouseEvent* e)
{
    LineRef line;
    int pos;
    convertToLinePos(e->x(), e->y(), line, pos);
    d->m_lastKnownMousePos = e->pos();

    if(d->m_selection.isValidFirstLine())
    {
        d->m_selection.end(line, pos);

        showStatusLine(line);

        // Scroll because mouse moved out of the window
        const QFontMetrics& fm = fontMetrics();
        int fontWidth = Utils::getHorizontalAdvance(fm, '0');
        int deltaX = 0;
        int deltaY = 0;
        if(!d->m_pOptions->m_bRightToLeftLanguage)
        {
            if(e->x() < d->leftInfoWidth() * fontWidth) deltaX = -1 - abs(e->x() - d->leftInfoWidth() * fontWidth) / fontWidth;
            if(e->x() > width()) deltaX = +1 + abs(e->x() - width()) / fontWidth;
        }
        else
        {
            if(e->x() > width() - 1 - d->leftInfoWidth() * fontWidth) deltaX = +1 + abs(e->x() - (width() - 1 - d->leftInfoWidth() * fontWidth)) / fontWidth;
            if(e->x() < fontWidth) deltaX = -1 - abs(e->x() - fontWidth) / fontWidth;
        }
        if(e->y() < 0) deltaY = -1 - (int)std::pow<int, int>(e->y(), 2) / (int)std::pow(fm.lineSpacing(), 2);
        if(e->y() > height()) deltaY = 1 + (int)std::pow(e->y() - height(), 2) / (int)std::pow(fm.lineSpacing(), 2);
        if((deltaX != 0 && d->m_scrollDeltaX != deltaX) || (deltaY != 0 && d->m_scrollDeltaY != deltaY))
        {
            d->m_scrollDeltaX = deltaX;
            d->m_scrollDeltaY = deltaY;
            emit scrollDiffTextWindow(deltaX, deltaY);
            if(d->m_delayedDrawTimer)
                killTimer(d->m_delayedDrawTimer);
            d->m_delayedDrawTimer = startTimer(50);
        }
        else
        {
            d->m_scrollDeltaX = deltaX;
            d->m_scrollDeltaY = deltaY;
            d->myUpdate(0);
        }
    }
}

void DiffTextWindowData::myUpdate(int afterMilliSecs)
{
    if(m_delayedDrawTimer)
        m_pDiffTextWindow->killTimer(m_delayedDrawTimer);
    m_bMyUpdate = true;
    m_delayedDrawTimer = m_pDiffTextWindow->startTimer(afterMilliSecs);
}

void DiffTextWindow::timerEvent(QTimerEvent*)
{
    killTimer(d->m_delayedDrawTimer);
    d->m_delayedDrawTimer = 0;

    if(d->m_bMyUpdate)
    {
        int fontHeight = fontMetrics().lineSpacing();

        if(d->m_selection.getOldLastLine().isValid())
        {
            int lastLine;
            int firstLine;
            if(d->m_selection.getOldFirstLine().isValid())
            {
                firstLine = min3(d->m_selection.getOldFirstLine(), d->m_selection.getLastLine(), d->m_selection.getOldLastLine());
                lastLine = max3(d->m_selection.getOldFirstLine(), d->m_selection.getLastLine(), d->m_selection.getOldLastLine());
            }
            else
            {
                firstLine = std::min(d->m_selection.getLastLine(), d->m_selection.getOldLastLine());
                lastLine = std::max(d->m_selection.getLastLine(), d->m_selection.getOldLastLine());
            }
            int y1 = (firstLine - d->m_firstLine) * fontHeight;
            int y2 = std::min(height(), (lastLine - d->m_firstLine + 1) * fontHeight);

            if(y1 < height() && y2 > 0)
            {
                QRect invalidRect = QRect(0, y1 - 1, width(), y2 - y1 + fontHeight); // Some characters in exotic exceed the regular bottom.
                update(invalidRect);
            }
        }

        d->m_bMyUpdate = false;
    }

    if(d->m_scrollDeltaX != 0 || d->m_scrollDeltaY != 0)
    {
        d->m_selection.end(d->m_selection.getLastLine() + d->m_scrollDeltaY, d->m_selection.getLastPos() + d->m_scrollDeltaX);
        emit scrollDiffTextWindow(d->m_scrollDeltaX, d->m_scrollDeltaY);
        killTimer(d->m_delayedDrawTimer);
        d->m_delayedDrawTimer = startTimer(50);
    }
}

void DiffTextWindow::resetSelection()
{
    d->m_selection.reset();
    update();
}

void DiffTextWindow::convertToLinePos(int x, int y, LineRef& line, int& pos)
{
    const QFontMetrics& fm = fontMetrics();
    int fontHeight = fm.lineSpacing();

    int yOffset = -d->m_firstLine * fontHeight;

    line = (y - yOffset) / fontHeight;
    if(line.isValid() && (!d->m_pOptions->m_bWordWrap || line < d->m_diff3WrapLineVector.count()))
    {
        QString s = d->getLineString(line);
        QTextLayout textLayout(s, font(), this);
        d->prepareTextLayout(textLayout, !d->m_pOptions->m_bWordWrap || d->m_diff3WrapLineVector[line].wrapLineOffset == 0);
        pos = textLayout.lineAt(0).xToCursor(x - textLayout.position().x());
    }
    else
        pos = -1;
}

class FormatRangeHelper
{
  private:
    QFont m_font;
    QPen m_pen;
    QColor m_background;
    int m_currentPos;

    QVector<QTextLayout::FormatRange> m_formatRanges;

  public:
    inline operator QVector<QTextLayout::FormatRange>() { return m_formatRanges; }
    FormatRangeHelper()
    {
        m_pen = QColor(Qt::black);
        m_background = QColor(Qt::white);
        m_currentPos = 0;
    }
    void setFont(const QFont& f)
    {
        m_font = f;
    }
    void setPen(const QPen& pen)
    {
        m_pen = pen;
    }
    void setBackground(const QColor& background)
    {
        m_background = background;
    }

    void next()
    {
        if(m_formatRanges.isEmpty() || m_formatRanges.back().format.foreground().color() != m_pen.color() || m_formatRanges.back().format.background().color() != m_background)
        {
            QTextLayout::FormatRange fr;
            fr.length = 1;
            fr.start = m_currentPos;
            fr.format.setForeground(m_pen.color());
            fr.format.setBackground(m_background);
            m_formatRanges.append(fr);
        }
        else
        {
            ++m_formatRanges.back().length;
        }
        ++m_currentPos;
    }
};

void DiffTextWindowData::prepareTextLayout(QTextLayout& textLayout, bool /*bFirstLine*/, int visibleTextWidth)
{
    QTextOption textOption;
#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)
    textOption.setTabStop(QFontMetricsF(m_pDiffTextWindow->font()).width(' ') * m_pOptions->m_tabSize);
#else
    textOption.setTabStopDistance(QFontMetricsF(m_pDiffTextWindow->font()).width(' ') * m_pOptions->m_tabSize);
#endif
    if(m_pOptions->m_bShowWhiteSpaceCharacters)
        textOption.setFlags(QTextOption::ShowTabsAndSpaces);
    if(m_pOptions->m_bRightToLeftLanguage)
        textOption.setAlignment(Qt::AlignRight); // only relevant for multi line text layout
    if(visibleTextWidth >= 0)
        textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);

    textLayout.setTextOption(textOption);

    if(m_pOptions->m_bShowWhiteSpaceCharacters)
    {
        // This additional format is only necessary for the tab arrow
        QVector<QTextLayout::FormatRange> formats;
        QTextLayout::FormatRange formatRange;
        formatRange.start = 0;
        formatRange.length = textLayout.text().length();
        formatRange.format.setFont(m_pDiffTextWindow->font());
        formats.append(formatRange);
        textLayout.setFormats(formats);
    }
    textLayout.beginLayout();

    int leading = m_pDiffTextWindow->fontMetrics().leading();
    int height = 0;

    int fontWidth = Utils::getHorizontalAdvance(m_pDiffTextWindow->fontMetrics(), '0');
    int xOffset = leftInfoWidth() * fontWidth - m_horizScrollOffset;
    int textWidth = visibleTextWidth;
    if(textWidth < 0)
        textWidth = m_pDiffTextWindow->width() - xOffset;

    int indentation = 0;
    while(true)
    {
        QTextLine line = textLayout.createLine();
        if(!line.isValid())
            break;

        height += leading;
        //if ( !bFirstLine )
        //   indentation = m_pDiffTextWindow->fontMetrics().width(' ') * m_pOptions->m_tabSize;
        if(visibleTextWidth >= 0)
        {
            line.setLineWidth(visibleTextWidth - indentation);
            line.setPosition(QPointF(indentation, height));
            height += qCeil(line.height());
            //bFirstLine = false;
        }
        else // only one line
        {
            line.setPosition(QPointF(indentation, height));
            break;
        }
    }

    textLayout.endLayout();
    if(m_pOptions->m_bRightToLeftLanguage)
        textLayout.setPosition(QPointF(textWidth - textLayout.maximumWidth(), 0));
    else
        textLayout.setPosition(QPointF(xOffset, 0));
}

void DiffTextWindowData::writeLine(
    RLPainter& p,
    const LineData* pld,
    const DiffList* pLineDiff1,
    const DiffList* pLineDiff2,
    const LineRef& line,
    const ChangeFlags whatChanged,
    const ChangeFlags whatChanged2,
    const LineRef& srcLineIdx,
    int wrapLineOffset,
    int wrapLineLength,
    bool bWrapLine,
    const QRect& invalidRect,
    int deviceWidth)
{
    QFont normalFont = p.font();

    const QFontMetrics& fm = p.fontMetrics();
    int fontHeight = fm.lineSpacing();
    int fontAscent = fm.ascent();
    int fontWidth = Utils::getHorizontalAdvance(fm, '0');

    int xOffset = leftInfoWidth() * fontWidth - m_horizScrollOffset;
    int yOffset = (line - m_firstLine) * fontHeight;

    QRect lineRect(xOffset, yOffset, deviceWidth, fontHeight);
    if(!invalidRect.intersects(lineRect))
    {
        return;
    }

    int fastSelectorLine1 = m_pDiffTextWindow->convertDiff3LineIdxToLine(m_fastSelectorLine1);
    int fastSelectorLine2 = m_pDiffTextWindow->convertDiff3LineIdxToLine(m_fastSelectorLine1 + m_fastSelectorNofLines) - 1;

    bool bFastSelectionRange = (line >= fastSelectorLine1 && line <= fastSelectorLine2);
    QColor bgColor = m_pOptions->m_bgColor;
    QColor diffBgColor = m_pOptions->m_diffBgColor;

    if(bFastSelectionRange)
    {
        bgColor = m_pOptions->m_currentRangeBgColor;
        diffBgColor = m_pOptions->m_currentRangeDiffBgColor;
    }

    if(yOffset + fontHeight < invalidRect.top() || invalidRect.bottom() < yOffset - fontHeight)
        return;

    ChangeFlags changed = whatChanged;
    if(pLineDiff1 != nullptr) changed |= AChanged;
    if(pLineDiff2 != nullptr) changed |= BChanged;

    QColor c = m_pOptions->m_fgColor;
    p.setPen(c);
    if(changed == BChanged)
    {
        c = m_cDiff2;
    }
    else if(changed == AChanged)
    {
        c = m_cDiff1;
    }
    else if(changed == Both)
    {
        c = m_cDiffBoth;
    }

    if(pld != nullptr)
    {
        // First calculate the "changed" information for each character.
        int i = 0;
        QString lineString = pld->getLine();
        if(!lineString.isEmpty())
        {
            switch(lineString[lineString.length() - 1].unicode())
            {
                case '\n':
                    lineString[lineString.length() - 1] = 0x00B6;
                    break; // "Pilcrow", "paragraph mark"
                case '\r':
                    lineString[lineString.length() - 1] = 0x00A4;
                    break; // Currency sign ;0x2761 "curved stem paragraph sign ornament"
                    //case '\0b' : lineString[lineString.length()-1] = 0x2756; break; // some other nice looking character
            }
        }
        QVector<ChangeFlags> charChanged(pld->size());
        Merger merger(pLineDiff1, pLineDiff2);
        while(!merger.isEndReached() && i < pld->size())
        {
            if(i < pld->size())
            {
                charChanged[i] = merger.whatChanged();
                ++i;
            }
            merger.next();
        }

        int outPos = 0;

        int lineLength = m_bWordWrap ? wrapLineOffset + wrapLineLength : lineString.length();

        FormatRangeHelper frh;

        for(i = wrapLineOffset; i < lineLength; ++i)
        {
            c = m_pOptions->m_fgColor;
            ChangeFlags cchanged = charChanged[i] | whatChanged;

            if(cchanged == BChanged)
            {
                c = m_cDiff2;
            }
            else if(cchanged == AChanged)
            {
                c = m_cDiff1;
            }
            else if(cchanged == Both)
            {
                c = m_cDiffBoth;
            }

            if(c != m_pOptions->m_fgColor && whatChanged2 == 0 && !m_pOptions->m_bShowWhiteSpace)
            {
                // The user doesn't want to see highlighted white space.
                c = m_pOptions->m_fgColor;
            }

            {
                frh.setBackground(bgColor);
                if(!m_selection.within(line, outPos))
                {

                    if(c != m_pOptions->m_fgColor)
                    {
                        QColor lightc = diffBgColor;
                        frh.setBackground(lightc);
                        // Setting italic font here doesn't work: Changing the font only when drawing is too late
                    }

                    frh.setPen(c);
                    frh.next();
                    frh.setFont(normalFont);
                }
                else
                {
                    frh.setBackground(m_pDiffTextWindow->palette().highlight().color());
                    frh.setPen(m_pDiffTextWindow->palette().highlightedText().color());
                    frh.next();

                    m_selection.bSelectionContainsData = true;
                }
            }

            ++outPos;
        } // end for

        QTextLayout textLayout(lineString.mid(wrapLineOffset, lineLength - wrapLineOffset), m_pDiffTextWindow->font(), m_pDiffTextWindow);
        prepareTextLayout(textLayout, !m_bWordWrap || wrapLineOffset == 0);
        textLayout.draw(&p, QPoint(0, yOffset), frh /*, const QRectF & clip = QRectF() */);
    }

    p.fillRect(0, yOffset, leftInfoWidth() * fontWidth, fontHeight, m_pOptions->m_bgColor);

    xOffset = (m_lineNumberWidth + 2) * fontWidth;
    int xLeft = m_lineNumberWidth * fontWidth;
    p.setPen(m_pOptions->m_fgColor);
    if(pld != nullptr)
    {
        if(m_pOptions->m_bShowLineNumbers && !bWrapLine)
        {
            QString num;
            num.sprintf("%0*d", m_lineNumberWidth, srcLineIdx + 1);
            p.drawText(0, yOffset + fontAscent, num);
            //p.drawLine( xLeft -1, yOffset, xLeft -1, yOffset+fontHeight-1 );
        }
        if(!bWrapLine || wrapLineLength > 0)
        {
            Qt::PenStyle wrapLinePenStyle = Qt::DotLine;

            p.setPen(QPen(m_pOptions->m_fgColor, 0, bWrapLine ? wrapLinePenStyle : Qt::SolidLine));
            p.drawLine(xOffset + 1, yOffset, xOffset + 1, yOffset + fontHeight - 1);
            p.setPen(QPen(m_pOptions->m_fgColor, 0, Qt::SolidLine));
        }
    }
    if(c != m_pOptions->m_fgColor && whatChanged2 == 0) //&& whatChanged==0 )
    {
        if(m_pOptions->m_bShowWhiteSpace)
        {
            p.setBrushOrigin(0, 0);
            p.fillRect(xLeft, yOffset, fontWidth * 2 - 1, fontHeight, QBrush(c, Qt::Dense5Pattern));
        }
    }
    else
    {
        p.fillRect(xLeft, yOffset, fontWidth * 2 - 1, fontHeight, c == m_pOptions->m_fgColor ? bgColor : c);
    }

    if(bFastSelectionRange)
    {
        p.fillRect(xOffset + fontWidth - 1, yOffset, 3, fontHeight, m_pOptions->m_fgColor);
    }

    // Check if line needs a manual diff help mark
    ManualDiffHelpList::const_iterator ci;
    for(ci = m_pManualDiffHelpList->begin(); ci != m_pManualDiffHelpList->end(); ++ci)
    {
        const ManualDiffHelpEntry& mdhe = *ci;
        LineRef rangeLine1;
        LineRef rangeLine2;

        mdhe.getRangeForUI(m_winIdx, &rangeLine1, &rangeLine2);
        if(rangeLine1.isValid() && rangeLine2.isValid() && srcLineIdx >= rangeLine1 && srcLineIdx <= rangeLine2)
        {
            p.fillRect(xOffset - fontWidth, yOffset, fontWidth - 1, fontHeight, m_pOptions->m_manualHelpRangeColor);
            break;
        }
    }
}

void DiffTextWindow::paintEvent(QPaintEvent* e)
{
    QRect invalidRect = e->rect();
    if(invalidRect.isEmpty())
        return;

    if(d->m_pDiff3LineVector == nullptr || (d->m_diff3WrapLineVector.empty() && d->m_bWordWrap))
    {
        QPainter p(this);
        p.fillRect(invalidRect, d->m_pOptions->m_bgColor);
        return;
    }

    bool bOldSelectionContainsData = d->m_selection.bSelectionContainsData;
    d->m_selection.bSelectionContainsData = false;

    int endLine = std::min(d->m_firstLine + getNofVisibleLines() + 2, getNofLines());

    RLPainter p(this, d->m_pOptions->m_bRightToLeftLanguage, width(), Utils::getHorizontalAdvance(fontMetrics(), '0'));

    p.setFont(font());
    p.QPainter::fillRect(invalidRect, d->m_pOptions->m_bgColor);

    d->draw(p, invalidRect, width(), d->m_firstLine, endLine);
    p.end();

    d->m_oldFirstLine = d->m_firstLine;
    d->m_selection.clearOldSelection();

    if(!bOldSelectionContainsData && d->m_selection.selectionContainsData())
        emit newSelection();
}

void DiffTextWindow::print(RLPainter& p, const QRect&, int firstLine, int nofLinesPerPage)
{
    if(d->m_pDiff3LineVector == nullptr || !updatesEnabled() ||
       (d->m_diff3WrapLineVector.empty() && d->m_bWordWrap))
        return;
    resetSelection();
    int oldFirstLine = d->m_firstLine;
    d->m_firstLine = firstLine;
    QRect invalidRect = QRect(0, 0, 1000000000, 1000000000);
    QColor bgColor = d->m_pOptions->m_bgColor;
    d->m_pOptions->m_bgColor = Qt::white;
    d->draw(p, invalidRect, p.window().width(), firstLine, std::min(firstLine + nofLinesPerPage, getNofLines()));
    d->m_pOptions->m_bgColor = bgColor;
    d->m_firstLine = oldFirstLine;
}

void DiffTextWindowData::draw(RLPainter& p, const QRect& invalidRect, int deviceWidth, int beginLine, int endLine)
{
    m_lineNumberWidth = m_pOptions->m_bShowLineNumbers ? (int)log10((double)std::max(m_size, 1)) + 1 : 0;

    if(m_winIdx == A)
    {
        m_cThis = m_pOptions->m_colorA;
        m_cDiff1 = m_pOptions->m_colorB;
        m_cDiff2 = m_pOptions->m_colorC;
    }
    if(m_winIdx == B)
    {
        m_cThis = m_pOptions->m_colorB;
        m_cDiff1 = m_pOptions->m_colorC;
        m_cDiff2 = m_pOptions->m_colorA;
    }
    if(m_winIdx == C)
    {
        m_cThis = m_pOptions->m_colorC;
        m_cDiff1 = m_pOptions->m_colorA;
        m_cDiff2 = m_pOptions->m_colorB;
    }
    m_cDiffBoth = m_pOptions->m_colorForConflict; // Conflict color

    p.setPen(m_cThis);

    for(int line = beginLine; line < endLine; ++line)
    {
        int wrapLineOffset = 0;
        int wrapLineLength = 0;
        const Diff3Line* d3l = nullptr;
        bool bWrapLine = false;
        if(m_bWordWrap)
        {
            Diff3WrapLine& d3wl = m_diff3WrapLineVector[line];
            wrapLineOffset = d3wl.wrapLineOffset;
            wrapLineLength = d3wl.wrapLineLength;
            d3l = d3wl.pD3L;
            bWrapLine = line > 0 && m_diff3WrapLineVector[line - 1].pD3L == d3l;
        }
        else
        {
            d3l = (*m_pDiff3LineVector)[line];
        }
        DiffList* pFineDiff1;
        DiffList* pFineDiff2;
        ChangeFlags changed = NoChange;
        ChangeFlags changed2 = NoChange;

        LineRef srcLineIdx;
        d3l->getLineInfo(m_winIdx, m_bTriple, srcLineIdx, pFineDiff1, pFineDiff2, changed, changed2);

        writeLine(
            p,                                                             // QPainter
            !srcLineIdx.isValid() ? nullptr : &(*m_pLineData)[srcLineIdx], // Text in this line
            pFineDiff1,
            pFineDiff2,
            line, // Line on the screen
            changed,
            changed2,
            srcLineIdx,
            wrapLineOffset,
            wrapLineLength,
            bWrapLine,
            invalidRect,
            deviceWidth);
    }
}

QString DiffTextWindowData::getString(int d3lIdx)
{
    if(d3lIdx < 0 || d3lIdx >= m_pDiff3LineVector->size())
        return QString();

    const Diff3Line* d3l = (*m_pDiff3LineVector)[d3lIdx];
    DiffList* pFineDiff1;
    DiffList* pFineDiff2;
    ChangeFlags changed = NoChange;
    ChangeFlags changed2 = NoChange;
    LineRef lineIdx;

    d3l->getLineInfo(m_winIdx, m_bTriple, lineIdx, pFineDiff1, pFineDiff2, changed, changed2);

    if(!lineIdx.isValid())
        return QString();

    return (*m_pLineData)[lineIdx].getLine();
}

QString DiffTextWindowData::getLineString(int line)
{
    if(m_bWordWrap)
    {
        if(line < m_diff3WrapLineVector.count())
        {
            int d3LIdx = m_pDiffTextWindow->convertLineToDiff3LineIdx(line);
            return getString(d3LIdx).mid(m_diff3WrapLineVector[line].wrapLineOffset, m_diff3WrapLineVector[line].wrapLineLength);
        }
        else
            return QString();
    }
    else
    {
        return getString(line);
    }
}

void DiffTextWindow::resizeEvent(QResizeEvent* e)
{
    QSize s = e->size();
    QFontMetrics fm = fontMetrics();
    int visibleLines = s.height() / fm.lineSpacing() - 2;
    int visibleColumns = s.width() / Utils::getHorizontalAdvance(fm, '0') - d->leftInfoWidth();
    if(e->size().height() != e->oldSize().height())
        emit resizeHeightChangedSignal(visibleLines);
    if(e->size().width() != e->oldSize().width())
        emit resizeWidthChangedSignal(visibleColumns);
    QWidget::resizeEvent(e);
}

int DiffTextWindow::getNofVisibleLines()
{
    QFontMetrics fm = fontMetrics();
    int fmh = fm.lineSpacing();
    int h = height();
    return h / fmh - 1;
}

int DiffTextWindow::getVisibleTextAreaWidth()
{
    QFontMetrics fm = fontMetrics();
    return width() - d->leftInfoWidth() * Utils::getHorizontalAdvance(fm, '0');
}

QString DiffTextWindow::getSelection()
{
    if(d->m_pLineData == nullptr)
        return QString();

    QString selectionString;

    int line = 0;
    int lineIdx = 0;

    int it;
    int vectorSize = d->m_bWordWrap ? d->m_diff3WrapLineVector.size() : d->m_pDiff3LineVector->size();
    for(it = 0; it < vectorSize; ++it)
    {
        const Diff3Line* d3l = d->m_bWordWrap ? d->m_diff3WrapLineVector[it].pD3L : (*d->m_pDiff3LineVector)[it];

        Q_ASSERT(d->m_winIdx >= 1 && d->m_winIdx <= 3);

        if(d->m_winIdx == A)
        {
            lineIdx = d3l->getLineA();
        }
        else if(d->m_winIdx == B)
        {
            lineIdx = d3l->getLineB();
        }
        else if(d->m_winIdx == C)
        {
            lineIdx = d3l->getLineC();
        }

        if(lineIdx != -1)
        {
            int size = (*d->m_pLineData)[lineIdx].size();
            QString lineString = (*d->m_pLineData)[lineIdx].getLine();

            if(d->m_bWordWrap)
            {
                size = d->m_diff3WrapLineVector[it].wrapLineLength;
                lineString = lineString.mid(d->m_diff3WrapLineVector[it].wrapLineOffset, size);
            }

            for(int i = 0; i < size; ++i)
            {
                if(d->m_selection.within(line, i))
                {
                    selectionString += lineString[i];
                }
            }

            if(d->m_selection.within(line, size) &&
               !(d->m_bWordWrap && it + 1 < vectorSize && d3l == d->m_diff3WrapLineVector[it + 1].pD3L))
            {
#if defined(Q_OS_WIN)
                selectionString += '\r';
#endif
                selectionString += '\n';
            }
        }

        ++line;
    }

    return selectionString;
}

bool DiffTextWindow::findString(const QString& s, LineRef& d3vLine, int& posInLine, bool bDirDown, bool bCaseSensitive)
{
    int it = d3vLine;
    int endIt = bDirDown ? d->m_pDiff3LineVector->size() : -1;
    int step = bDirDown ? 1 : -1;
    int startPos = posInLine;

    for(; it != endIt; it += step)
    {
        QString line = d->getString(it);
        if(!line.isEmpty())
        {
            int pos = line.indexOf(s, startPos, bCaseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive);
            if(pos != -1)
            {
                d3vLine = it;
                posInLine = pos;
                return true;
            }

            startPos = 0;
        }
    }
    return false;
}

void DiffTextWindow::convertD3LCoordsToLineCoords(int d3LIdx, int d3LPos, int& line, int& pos)
{
    if(d->m_bWordWrap)
    {
        int wrapPos = d3LPos;
        int wrapLine = convertDiff3LineIdxToLine(d3LIdx);
        while(wrapPos > d->m_diff3WrapLineVector[wrapLine].wrapLineLength)
        {
            wrapPos -= d->m_diff3WrapLineVector[wrapLine].wrapLineLength;
            ++wrapLine;
        }
        pos = wrapPos;
        line = wrapLine;
    }
    else
    {
        pos = d3LPos;
        line = d3LIdx;
    }
}

void DiffTextWindow::convertLineCoordsToD3LCoords(int line, int pos, int& d3LIdx, int& d3LPos)
{
    if(d->m_bWordWrap)
    {
        d3LPos = pos;
        d3LIdx = convertLineToDiff3LineIdx(line);
        int wrapLine = convertDiff3LineIdxToLine(d3LIdx); // First wrap line belonging to this d3LIdx
        while(wrapLine < line)
        {
            d3LPos += d->m_diff3WrapLineVector[wrapLine].wrapLineLength;
            ++wrapLine;
        }
    }
    else
    {
        d3LPos = pos;
        d3LIdx = line;
    }
}

void DiffTextWindow::setSelection(LineRef firstLine, int startPos, LineRef lastLine, int endPos, LineRef& l, int& p)
{
    d->m_selection.reset();
    if(lastLine >= getNofLines())
    {
        lastLine = getNofLines() - 1;

        const Diff3Line* d3l = (*d->m_pDiff3LineVector)[convertLineToDiff3LineIdx(lastLine)];
        LineRef line;
        if(d->m_winIdx == A) line = d3l->getLineA();
        if(d->m_winIdx == B) line = d3l->getLineB();
        if(d->m_winIdx == C) line = d3l->getLineC();
        if(line.isValid())
            endPos = (*d->m_pLineData)[line].width(d->m_pOptions->m_tabSize);
    }

    if(d->m_bWordWrap && d->m_pDiff3LineVector != nullptr)
    {
        QString s1 = d->getString(firstLine);
        int firstWrapLine = convertDiff3LineIdxToLine(firstLine);
        int wrapStartPos = startPos;
        while(wrapStartPos > d->m_diff3WrapLineVector[firstWrapLine].wrapLineLength)
        {
            wrapStartPos -= d->m_diff3WrapLineVector[firstWrapLine].wrapLineLength;
            s1 = s1.mid(d->m_diff3WrapLineVector[firstWrapLine].wrapLineLength);
            ++firstWrapLine;
        }

        QString s2 = d->getString(lastLine);
        int lastWrapLine = convertDiff3LineIdxToLine(lastLine);
        int wrapEndPos = endPos;
        while(wrapEndPos > d->m_diff3WrapLineVector[lastWrapLine].wrapLineLength)
        {
            wrapEndPos -= d->m_diff3WrapLineVector[lastWrapLine].wrapLineLength;
            s2 = s2.mid(d->m_diff3WrapLineVector[lastWrapLine].wrapLineLength);
            ++lastWrapLine;
        }

        d->m_selection.start(firstWrapLine, wrapStartPos);
        d->m_selection.end(lastWrapLine, wrapEndPos);
        l = firstWrapLine;
        p = wrapStartPos;
    }
    else
    {
        if(d->m_pDiff3LineVector != nullptr)
        {
            d->m_selection.start(firstLine, startPos);
            d->m_selection.end(lastLine, endPos);
            l = firstLine;
            p = startPos;
        }
    }
    update();
}

int DiffTextWindowData::convertLineOnScreenToLineInSource(int lineOnScreen, e_CoordType coordType, bool bFirstLine)
{
    LineRef line = -1;
    if(lineOnScreen >= 0)
    {
        if(coordType == eWrapCoords) return lineOnScreen;
        int d3lIdx = m_pDiffTextWindow->convertLineToDiff3LineIdx(lineOnScreen);
        if(!bFirstLine && d3lIdx >= m_pDiff3LineVector->size())
            d3lIdx = m_pDiff3LineVector->size() - 1;
        if(coordType == eD3LLineCoords) return d3lIdx;
        while(line < 0 && d3lIdx < m_pDiff3LineVector->size() && d3lIdx >= 0)
        {
            const Diff3Line* d3l = (*m_pDiff3LineVector)[d3lIdx];
            if(m_winIdx == A) line = d3l->getLineA();
            if(m_winIdx == B) line = d3l->getLineB();
            if(m_winIdx == C) line = d3l->getLineC();
            if(bFirstLine)
                ++d3lIdx;
            else
                --d3lIdx;
        }
        if(coordType == eFileCoords) return line;
    }
    return line;
}

void DiffTextWindow::getSelectionRange(LineRef* pFirstLine, LineRef* pLastLine, e_CoordType coordType)
{
    if(pFirstLine)
        *pFirstLine = d->convertLineOnScreenToLineInSource(d->m_selection.beginLine(), coordType, true);
    if(pLastLine)
        *pLastLine = d->convertLineOnScreenToLineInSource(d->m_selection.endLine(), coordType, false);
}

void DiffTextWindow::convertSelectionToD3LCoords()
{
    if(d->m_pDiff3LineVector == nullptr || !updatesEnabled() || !isVisible() || d->m_selection.isEmpty())
    {
        return;
    }

    // convert the d->m_selection to unwrapped coordinates: Later restore to new coords
    int firstD3LIdx, firstD3LPos;
    QString s = d->getLineString(d->m_selection.beginLine());
    int firstPosInText = d->m_selection.beginPos();
    convertLineCoordsToD3LCoords(d->m_selection.beginLine(), firstPosInText, firstD3LIdx, firstD3LPos);

    int lastD3LIdx, lastD3LPos;
    s = d->getLineString(d->m_selection.endLine());
    int lastPosInText = d->m_selection.endPos();
    convertLineCoordsToD3LCoords(d->m_selection.endLine(), lastPosInText, lastD3LIdx, lastD3LPos);

    d->m_selection.start(firstD3LIdx, firstD3LPos);
    d->m_selection.end(lastD3LIdx, lastD3LPos);
}

bool DiffTextWindow::startRunnables()
{
    if(s_runnables.count() == 0)
    {
        return false;
    }
    else
    {
        g_pProgressDialog->setStayHidden(true);
        g_pProgressDialog->push();
        g_pProgressDialog->setMaxNofSteps(s_runnables.count());
        RecalcWordWrapRunnable::s_maxNofRunnables = s_runnables.count();
        g_pProgressDialog->setCurrent(0);

        for(int i = 0; i < s_runnables.count(); ++i)
        {
            QThreadPool::globalInstance()->start(s_runnables[i]);
        }

        s_runnables.clear();
        return true;
    }
}

void DiffTextWindow::recalcWordWrap(bool bWordWrap, int wrapLineVectorSize, int visibleTextWidth)
{
    if(d->m_pDiff3LineVector == nullptr || !isVisible())
    {
        d->m_bWordWrap = bWordWrap;
        if(!bWordWrap) d->m_diff3WrapLineVector.resize(0);
        return;
    }

    d->m_bWordWrap = bWordWrap;

    if(bWordWrap)
    {
        d->m_lineNumberWidth = d->m_pOptions->m_bShowLineNumbers ? (int)log10((double)std::max(d->m_size, 1)) + 1 : 0;

        d->m_diff3WrapLineVector.resize(wrapLineVectorSize);

        if(wrapLineVectorSize == 0)
        {
            d->m_wrapLineCacheList.clear();
            setUpdatesEnabled(false);
            for(int i = 0, j = 0; i < d->m_pDiff3LineVector->size(); i += s_linesPerRunnable, ++j)
            {
                d->m_wrapLineCacheList.append(QVector<WrapLineCacheData>());
                s_runnables.push_back(new RecalcWordWrapRunnable(this, visibleTextWidth, j));
            }
        }
        else
        {
            recalcWordWrapHelper(wrapLineVectorSize, visibleTextWidth, 0);
            setUpdatesEnabled(true);
        }
    }
    else
    {
        if(wrapLineVectorSize == 0 && getAtomic(d->m_maxTextWidth) < 0)
        {
            d->m_diff3WrapLineVector.resize(0);
            d->m_wrapLineCacheList.clear();
            setUpdatesEnabled(false);
            for(int i = 0, j = 0; i < d->m_pDiff3LineVector->size(); i += s_linesPerRunnable, ++j)
            {
                s_runnables.push_back(new RecalcWordWrapRunnable(this, visibleTextWidth, j));
            }
        }
        else
        {
            setUpdatesEnabled(true);
        }
    }
}

void DiffTextWindow::recalcWordWrapHelper(int wrapLineVectorSize, int visibleTextWidth, int cacheListIdx)
{
    if(d->m_bWordWrap)
    {
        if(g_pProgressDialog->wasCancelled())
            return;
        if(visibleTextWidth < 0)
            visibleTextWidth = getVisibleTextAreaWidth();
        else
            visibleTextWidth -= d->leftInfoWidth() * Utils::getHorizontalAdvance(fontMetrics(), '0');
        int i;
        int wrapLineIdx = 0;
        int size = d->m_pDiff3LineVector->size();
        int firstD3LineIdx = wrapLineVectorSize > 0 ? 0 : cacheListIdx * s_linesPerRunnable;
        int endIdx = wrapLineVectorSize > 0 ? size : std::min(firstD3LineIdx + s_linesPerRunnable, size);
        QVector<WrapLineCacheData>& wrapLineCache = d->m_wrapLineCacheList[cacheListIdx];
        int cacheListIdx2 = 0;
        QTextLayout textLayout(QString(), font(), this);
        for(i = firstD3LineIdx; i < endIdx; ++i)
        {
            if(g_pProgressDialog->wasCancelled())
                return;

            int linesNeeded = 0;
            if(wrapLineVectorSize == 0)
            {
                QString s = d->getString(i);
                textLayout.clearLayout();
                textLayout.setText(s);
                d->prepareTextLayout(textLayout, true, visibleTextWidth);
                linesNeeded = textLayout.lineCount();
                for(int l = 0; l < linesNeeded; ++l)
                {
                    QTextLine line = textLayout.lineAt(l);
                    wrapLineCache.push_back(WrapLineCacheData(i, line.textStart(), line.textLength()));
                }
            }
            else if(wrapLineVectorSize > 0 && cacheListIdx2 < d->m_wrapLineCacheList.count())
            {
                WrapLineCacheData* pWrapLineCache = d->m_wrapLineCacheList[cacheListIdx2].data();
                int cacheIdx = 0;
                int clc = d->m_wrapLineCacheList.count() - 1;
                int cllc = d->m_wrapLineCacheList.last().count();
                int curCount = d->m_wrapLineCacheList[cacheListIdx2].count() - 1;
                int l = 0;
                while((cacheListIdx2 < clc || (cacheListIdx2 == clc && cacheIdx < cllc)) && pWrapLineCache->d3LineIdx() <= i)
                {
                    if(pWrapLineCache->d3LineIdx() == i)
                    {
                        Diff3WrapLine* pDiff3WrapLine = &d->m_diff3WrapLineVector[wrapLineIdx + l];
                        pDiff3WrapLine->wrapLineOffset = pWrapLineCache->textStart();
                        pDiff3WrapLine->wrapLineLength = pWrapLineCache->textLength();
                        ++l;
                    }
                    if(cacheIdx < curCount)
                    {
                        ++cacheIdx;
                        ++pWrapLineCache;
                    }
                    else
                    {
                        ++cacheListIdx2;
                        if(cacheListIdx2 >= d->m_wrapLineCacheList.count())
                            break;
                        pWrapLineCache = d->m_wrapLineCacheList[cacheListIdx2].data();
                        curCount = d->m_wrapLineCacheList[cacheListIdx2].count();
                        cacheIdx = 0;
                    }
                }
                linesNeeded = l;
            }

            Diff3Line& d3l = *(*d->m_pDiff3LineVector)[i];
            if(d3l.linesNeededForDisplay < linesNeeded)
            {
                Q_ASSERT(wrapLineVectorSize == 0);
                d3l.linesNeededForDisplay = linesNeeded;
            }

            if(wrapLineVectorSize > 0)
            {
                int j;
                for(j = 0; j < d3l.linesNeededForDisplay; ++j, ++wrapLineIdx)
                {
                    Diff3WrapLine& d3wl = d->m_diff3WrapLineVector[wrapLineIdx];
                    d3wl.diff3LineIndex = i;
                    d3wl.pD3L = (*d->m_pDiff3LineVector)[i];
                    if(j >= linesNeeded)
                    {
                        d3wl.wrapLineOffset = 0;
                        d3wl.wrapLineLength = 0;
                    }
                }
            }
        }

        if(wrapLineVectorSize > 0)
        {
            d->m_firstLine = std::min(d->m_firstLine, wrapLineVectorSize - 1);
            d->m_horizScrollOffset = 0;
            d->m_pDiffTextWindowFrame->setFirstLine(d->m_firstLine);
        }
    }
    else // no word wrap, just calc the maximum text width
    {
        if(g_pProgressDialog->wasCancelled())
            return;
        int size = d->m_pDiff3LineVector->size();
        int firstD3LineIdx = cacheListIdx * s_linesPerRunnable;
        int endIdx = std::min(firstD3LineIdx + s_linesPerRunnable, size);

        int maxTextWidth = getAtomic(d->m_maxTextWidth); // current value
        QTextLayout textLayout(QString(), font(), this);
        for(int i = firstD3LineIdx; i < endIdx; ++i)
        {
            if(g_pProgressDialog->wasCancelled())
                return;
            textLayout.clearLayout();
            textLayout.setText(d->getString(i));
            d->prepareTextLayout(textLayout, true);
            if(textLayout.maximumWidth() > maxTextWidth)
                maxTextWidth = qCeil(textLayout.maximumWidth());
        }

        for(;;)
        {
            int prevMaxTextWidth = d->m_maxTextWidth.fetchAndStoreOrdered(maxTextWidth);
            if(prevMaxTextWidth <= maxTextWidth)
                break;
            maxTextWidth = prevMaxTextWidth;
        }
    }

    if(!d->m_selection.isEmpty() && (!d->m_bWordWrap || wrapLineVectorSize > 0))
    {
        // Assume unwrapped coordinates
        //( Why? ->Conversion to unwrapped coords happened a few lines above in this method.
        //  Also see KDiff3App::recalcWordWrap() on the role of wrapLineVectorSize)

        // Wrap them now.

        // convert the d->m_selection to unwrapped coordinates.
        int firstLine, firstPos;
        convertD3LCoordsToLineCoords(d->m_selection.beginLine(), d->m_selection.beginPos(), firstLine, firstPos);

        int lastLine, lastPos;
        convertD3LCoordsToLineCoords(d->m_selection.endLine(), d->m_selection.endPos(), lastLine, lastPos);

        d->m_selection.start(firstLine, firstPos);
        d->m_selection.end(lastLine, lastPos);
    }
}

class DiffTextWindowFrameData
{
  public:
    DiffTextWindowFrameData(DiffTextWindowFrame* frame, QStatusBar* pStatusBar, const QSharedPointer<Options>& pOptions, const e_SrcSelector winIdx)
    {
        m_winIdx = winIdx;

        m_pOptions = pOptions;
        m_pTopLineWidget = new QWidget(frame);
        m_pFileSelection = new FileNameLineEdit(m_pTopLineWidget);
        m_pBrowseButton = new QPushButton("...", m_pTopLineWidget);
        m_pBrowseButton->setFixedWidth(30);

        m_pFileSelection->setAcceptDrops(true);

        m_pLabel = new QLabel("A:", m_pTopLineWidget);
        m_pTopLine = new QLabel(m_pTopLineWidget);
        m_pDiffTextWindow = new DiffTextWindow(frame, pStatusBar, pOptions, winIdx);
    }

    const QPushButton* getBrowseButton() const { return m_pBrowseButton; }
    const FileNameLineEdit* getFileSelectionField() const { return m_pFileSelection; }
    const QWidget* getTopLineWidget() const { return m_pTopLineWidget; }
    const QLabel* getLabel() const { return m_pLabel; }

  private:
    friend DiffTextWindowFrame;
    DiffTextWindow* m_pDiffTextWindow;
    FileNameLineEdit* m_pFileSelection;
    QPushButton* m_pBrowseButton;
    QSharedPointer<Options> m_pOptions;
    QLabel* m_pLabel;
    QLabel* m_pTopLine;
    QLabel* m_pEncoding;
    QLabel* m_pLineEndStyle;
    QWidget* m_pTopLineWidget;
    e_SrcSelector m_winIdx;
};

DiffTextWindowFrame::DiffTextWindowFrame(QWidget* pParent, QStatusBar* pStatusBar, const QSharedPointer<Options>& pOptions, e_SrcSelector winIdx, SourceData* psd)
    : QWidget(pParent)
{
    d = new DiffTextWindowFrameData(this, pStatusBar, pOptions, winIdx);
    setAutoFillBackground(true);
    connect(d->getBrowseButton(), &QPushButton::clicked, this, &DiffTextWindowFrame::slotBrowseButtonClicked);
    connect(d->getFileSelectionField(), &QLineEdit::returnPressed, this, &DiffTextWindowFrame::slotReturnPressed);

    QVBoxLayout* pVTopLayout = new QVBoxLayout(const_cast<QWidget*>(d->getTopLineWidget()));
    pVTopLayout->setMargin(2);
    pVTopLayout->setSpacing(0);
    QHBoxLayout* pHL = new QHBoxLayout();
    QHBoxLayout* pHL2 = new QHBoxLayout();
    pVTopLayout->addLayout(pHL);
    pVTopLayout->addLayout(pHL2);

    // Upper line:
    pHL->setMargin(0);
    pHL->setSpacing(2);

    pHL->addWidget(const_cast<QLabel*>(d->getLabel()), 0);
    pHL->addWidget(const_cast<FileNameLineEdit*>(d->getFileSelectionField()), 1);
    pHL->addWidget(const_cast<QPushButton*>(d->getBrowseButton()), 0);
    pHL->addWidget(d->m_pTopLine, 0);

    // Lower line
    pHL2->setMargin(0);
    pHL2->setSpacing(2);
    pHL2->addWidget(d->m_pTopLine, 0);
    d->m_pEncoding = new EncodingLabel(i18n("Encoding:"), psd, pOptions);
    //EncodeLabel::EncodingChanged should be handled asyncroniously.
    connect((EncodingLabel*)d->m_pEncoding, &EncodingLabel::encodingChanged, this, &DiffTextWindowFrame::slotEncodingChanged, Qt::QueuedConnection);

    d->m_pLineEndStyle = new QLabel(i18n("Line end style:"));
    pHL2->addWidget(d->m_pEncoding);
    pHL2->addWidget(d->m_pLineEndStyle);

    QVBoxLayout* pVL = new QVBoxLayout(this);
    pVL->setMargin(0);
    pVL->setSpacing(0);
    pVL->addWidget(const_cast<QWidget*>(d->getTopLineWidget()), 0);
    pVL->addWidget(d->m_pDiffTextWindow, 1);

    d->m_pDiffTextWindow->installEventFilter(this);
    d->m_pFileSelection->installEventFilter(this);
    d->m_pBrowseButton->installEventFilter(this);
    init();
}

DiffTextWindowFrame::~DiffTextWindowFrame()
{
    delete d;
}

void DiffTextWindowFrame::init()
{
    DiffTextWindow* pDTW = d->m_pDiffTextWindow;
    if(pDTW)
    {
        QString s = QDir::toNativeSeparators(pDTW->getFileName());
        d->m_pFileSelection->setText(s);
        QString winId = pDTW->getWindowIndex() == A ? (pDTW->isThreeWay() ? i18n("A (Base)") : i18n("A")) : (pDTW->getWindowIndex() == B ? i18n("B") : i18n("C"));
        d->m_pLabel->setText(winId + ':');
        d->m_pEncoding->setText(i18n("Encoding: %1", pDTW->getEncodingDisplayString()));
        d->m_pLineEndStyle->setText(i18n("Line end style: %1", pDTW->getLineEndStyle() == eLineEndStyleDos ? i18n("DOS") : i18n("Unix")));
    }
}

// Search for the first visible line (search loop needed when no line exists for this file.)
LineRef DiffTextWindow::calcTopLineInFile(const LineRef firstLine)
{
    LineRef currentLine;
    for(int i = convertLineToDiff3LineIdx(firstLine); i < (int)d->m_pDiff3LineVector->size(); ++i)
    {
        const Diff3Line* d3l = (*d->m_pDiff3LineVector)[i];
        currentLine = d3l->getLineInFile(d->m_winIdx);
        if(currentLine.isValid()) break;
    }
    return currentLine;
}

void DiffTextWindowFrame::setFirstLine(QtNumberType firstLine)
{
    DiffTextWindow* pDTW = d->m_pDiffTextWindow;
    if(pDTW && pDTW->getDiff3LineVector())
    {
        QString s = i18n("Top line");
        int lineNumberWidth = pDTW->getLineNumberWidth();

        LineRef topVisiableLine = pDTW->calcTopLineInFile(firstLine);

        int w = Utils::getHorizontalAdvance(d->m_pTopLine->fontMetrics(), s + ' ' + QString().fill('0', lineNumberWidth));
        d->m_pTopLine->setMinimumWidth(w);

        if(!topVisiableLine.isValid())
            s = i18n("End");
        else
            s += ' ' + QString::number(topVisiableLine + 1);

        d->m_pTopLine->setText(s);
        d->m_pTopLine->repaint();
    }
}

DiffTextWindow* DiffTextWindowFrame::getDiffTextWindow()
{
    return d->m_pDiffTextWindow;
}

bool DiffTextWindowFrame::eventFilter(QObject* o, QEvent* e)
{
    Q_UNUSED(o);
    if(e->type() == QEvent::FocusIn || e->type() == QEvent::FocusOut)
    {
        QColor c1 = d->m_pOptions->m_bgColor;
        QColor c2;
        if(d->m_winIdx == A)
            c2 = d->m_pOptions->m_colorA;
        else if(d->m_winIdx == B)
            c2 = d->m_pOptions->m_colorB;
        else if(d->m_winIdx == C)
            c2 = d->m_pOptions->m_colorC;

        QPalette p = d->m_pTopLineWidget->palette();
        if(e->type() == QEvent::FocusOut)
            std::swap(c1, c2);

        p.setColor(QPalette::Window, c2);
        setPalette(p);

        p.setColor(QPalette::WindowText, c1);
        d->m_pLabel->setPalette(p);
        d->m_pTopLine->setPalette(p);
        d->m_pEncoding->setPalette(p);
        d->m_pLineEndStyle->setPalette(p);
    }

    return false;
}

void DiffTextWindowFrame::slotReturnPressed()
{
    DiffTextWindow* pDTW = d->m_pDiffTextWindow;
    if(pDTW->getFileName() != d->m_pFileSelection->text())
    {
        emit fileNameChanged(d->m_pFileSelection->text(), pDTW->getWindowIndex());
    }
}

void DiffTextWindowFrame::slotBrowseButtonClicked()
{
    QString current = d->m_pFileSelection->text();

    QUrl newURL = QFileDialog::getOpenFileUrl(this, i18n("Open File"), QUrl::fromUserInput(current, QString(), QUrl::AssumeLocalFile));
    if(!newURL.isEmpty())
    {
        DiffTextWindow* pDTW = d->m_pDiffTextWindow;
        emit fileNameChanged(newURL.url(), pDTW->getWindowIndex());
    }
}

EncodingLabel::EncodingLabel(const QString& text, SourceData* pSD, const QSharedPointer<Options>& pOptions)
    : QLabel(text)
{
    m_pOptions = pOptions;
    m_pSourceData = pSD;
    m_pContextEncodingMenu = nullptr;
    setMouseTracking(true);
}

void EncodingLabel::mouseMoveEvent(QMouseEvent*)
{
    // When there is no data to display or it came from clipboard,
    // we will be use UTF-8 only,
    // in that case there is no possibility to change the encoding in the SourceData
    // so, we should hide the HandCursor and display usual ArrowCursor
    if(m_pSourceData->isFromBuffer() || m_pSourceData->isEmpty())
        setCursor(QCursor(Qt::ArrowCursor));
    else
        setCursor(QCursor(Qt::PointingHandCursor));
}

void EncodingLabel::mousePressEvent(QMouseEvent*)
{
    if(!(m_pSourceData->isFromBuffer() || m_pSourceData->isEmpty()))
    {
        delete m_pContextEncodingMenu;
        m_pContextEncodingMenu = new QMenu(this);
        QMenu* pContextEncodingSubMenu = new QMenu(m_pContextEncodingMenu);

        int currentTextCodecEnum = m_pSourceData->getEncoding()->mibEnum(); // the codec that will be checked in the context menu
        QList<int> mibs = QTextCodec::availableMibs();
        QList<int> codecEnumList;

        // Adding "main" encodings
        insertCodec(i18n("Unicode, 8 bit"), QTextCodec::codecForName("UTF-8"), codecEnumList, m_pContextEncodingMenu, currentTextCodecEnum);
        if(QTextCodec::codecForName("System"))
        {
            insertCodec(QString(), QTextCodec::codecForName("System"), codecEnumList, m_pContextEncodingMenu, currentTextCodecEnum);
        }

        // Adding recent encodings
        if(m_pOptions != nullptr)
        {
            QStringList& recentEncodings = m_pOptions->m_recentEncodings;
            for(const QString& s : recentEncodings)
            {
                insertCodec("", QTextCodec::codecForName(s.toLatin1()), codecEnumList, m_pContextEncodingMenu, currentTextCodecEnum);
            }
        }
        // Submenu to add the rest of available encodings
        pContextEncodingSubMenu->setTitle(i18n("Other"));
        for(int i : mibs)
        {
            QTextCodec* c = QTextCodec::codecForMib(i);
            if(c != nullptr)
                insertCodec("", c, codecEnumList, pContextEncodingSubMenu, currentTextCodecEnum);
        }

        m_pContextEncodingMenu->addMenu(pContextEncodingSubMenu);

        m_pContextEncodingMenu->exec(QCursor::pos());
    }
}

void EncodingLabel::insertCodec(const QString& visibleCodecName, QTextCodec* pCodec, QList<int>& codecEnumList, QMenu* pMenu, int currentTextCodecEnum)
{
    int CodecMIBEnum = pCodec->mibEnum();
    if(pCodec != nullptr && !codecEnumList.contains(CodecMIBEnum))
    {
        QAction* pAction = new QAction(pMenu); // menu takes ownership, so deleting the menu deletes the action too.
        QByteArray nameArray = pCodec->name();
        QLatin1String codecName = QLatin1String(nameArray);

        pAction->setText(visibleCodecName.isEmpty() ? codecName : visibleCodecName + QLatin1String(" (") + codecName + QLatin1String(")"));
        pAction->setData(CodecMIBEnum);
        pAction->setCheckable(true);
        if(currentTextCodecEnum == CodecMIBEnum)
            pAction->setChecked(true);
        pMenu->addAction(pAction);
        connect(pAction, &QAction::triggered, this, &EncodingLabel::slotSelectEncoding);
        codecEnumList.append(CodecMIBEnum);
    }
}

void EncodingLabel::slotSelectEncoding()
{
    QAction* pAction = qobject_cast<QAction*>(sender());
    if(pAction)
    {
        QTextCodec* pCodec = QTextCodec::codecForMib(pAction->data().toInt());
        if(pCodec != nullptr)
        {
            QString s(QLatin1String(pCodec->name()));
            QStringList& recentEncodings = m_pOptions->m_recentEncodings;
            if(!recentEncodings.contains(s) && s != "UTF-8" && s != "System")
            {
                int itemsToRemove = recentEncodings.size() - m_maxRecentEncodings + 1;
                for(int i = 0; i < itemsToRemove; ++i)
                {
                    recentEncodings.removeFirst();
                }
                recentEncodings.append(s);
            }
        }

        emit encodingChanged(pCodec);
    }
}
