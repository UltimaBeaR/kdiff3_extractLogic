#ifndef KDIFF3_MERGEDATAOBJ_H
#define KDIFF3_MERGEDATAOBJ_H

// Эти инклюды и предефайны взял с окошка (не факт что они все нужны) просто чтобы потом не париться при реализации.
// Потом в итоге нужно будет поубирать ненужное
// --------------------------------------------------------------------

#include "EXTRACT/2_final/MergeEditLine.h"
#include "EXTRACT/2_final/diff.h"
#include "FileNameLineEdit.h"
#include "Overview.h"

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

// --------------------------------------------------------------------

class MergeDataObj
{
    // Использование - сначала инициализация - Создание, init_setOptions(), init()
    // Далее вызов merge() для мержа из диффов и решения конфликтов
    // На этом моменте внутренние данные заполняются чем-то и получается состояние окошка мержа после прохода авто-мержа первоначального (с авто разрешением конфликтов или без в зависимости от того был ли флаг).
    // Далее можно вызывать choose() для разрешения конфликтов на одной из строчек (текущая строчка находится в m_currentMergeLineIt - надо еще понять как ее програмно изменять правильно).
    // choose() из ui дергается при нажатии на кнопку выбора какую строчку посдтавить (A,B,C) для решения конфликта.
    // Еще каким то образом можно вроде просто текст редактирвоать поверх уже решенных конфликтов - надо тоже найти как это делается из UI и тут это сделать видимо.
    // После того как все конфликты решены с помощью рук (choose() и явное редактирование текста) - можно сохранить результаты мержа в файл.
    // сохранения тут нету, но есть получение данных для сохранения в виде байтов файла - это getOutFileData()


  public:
    MergeDataObj();

    // Вызываю только в конструкторе создания MergeResultWindow - там единственное что нужно оттуда это эти опции, все остальное UI-ное.
    void init_setOptions(const MyOptions* pMyOptions);

    void init(
        const QVector<LineData>* pLineDataA, LineRef sizeA,
        const QVector<LineData>* pLineDataB, LineRef sizeB,
        const QVector<LineData>* pLineDataC, LineRef sizeC,
        const Diff3LineList* pDiff3LineList,
        TotalDiffStatus* pTotalDiffStatus);

    // Процесс мержа с возможностью авто-мержа. После этого действия идет ручное решение конфликтов.
    void merge(bool bAutoSolve, e_SrcSelector defaultSelector, bool bConflictsOnly, bool bWhiteSpaceOnly, std::function<void()> callback1, std::function<void()> callback2);

    // Выбрать один из вариантов решения конфликта на текущей строчке (текущая строчка сидит в m_currentMergeLineIt)
    void choose(e_SrcSelector selector);

    // Проходится по данным m_mergeLineList и считает кол-во конфликтов
    int getNrOfUnsolvedConflicts(int* pNrOfWhiteSpaceConflicts = nullptr);

    // Выдает представление m_mergeLineList в виде итогового файла (смерженный файл) в виде out буффера.
    bool getOutFileData(QTextCodec* pEncoding, e_LineEndStyle eLineEndStyle, void* &out_pBuffer, qint64 &out_bufferLength);
  public:

    void reset();

    inline void clearMergeList()
    {
        m_mergeLineList.clear();
    }

    // Получить строку по номеру строки в итоговом мерже (либо пустая строка в случае неудачи)
    QString getString(int lineIdx);

    // Получает итераторы на внутренние данные из номера строки
    bool calcIteratorFromLineNr(
        int line,
        MergeLineList::iterator& mlIt,
        MergeEditLineList::iterator& melIt);

    bool isItAtEnd(bool bIncrement, MergeLineList::iterator i)
    {
        if(bIncrement)
            return i != m_mergeLineList.end();
        else
            return i != m_mergeLineList.begin();
    }

    // TODO: пока на время переходного рефакторинга сделал все данные публичными (кроме тех что точно используются сейчас только внутри),
    // как только закончу с этим - нужно все сделать приватным (когда использования будут только внутри этого класса)
  public:
    // По использованию похоже что внутри там что-то типа версии итогово окошка с конфликтами но в виде данных.
    MergeLineList m_mergeLineList;

    const QVector<LineData>* m_pldA = nullptr;
    const QVector<LineData>* m_pldB = nullptr;
    const QVector<LineData>* m_pldC = nullptr;
    LineRef m_sizeA = 0;
    LineRef m_sizeB = 0;
    LineRef m_sizeC = 0;

    // Эти штуки ставятся при создании уже готовые - берутся как я понимаю как результат сделанных перед мержем диффов
    const Diff3LineList* m_pDiff3LineList = nullptr;
    TotalDiffStatus* m_pTotalDiffStatus = nullptr;

    // ПОХОЖЕ что это что-то типа текущей строки с которой идет работа.
    MergeLineList::iterator m_currentMergeLineIt;
  private:
    const MyOptions* m_pMyOptions;

    static bool sameKindCheck(const MergeLine& ml1, const MergeLine& ml2);
};

#endif // KDIFF3_MERGEDATAOBJ_H
