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
  public:
    MergeDataObj();

    // Похоже на данные.
    // Используется в получении кол-ва конфликтов
    // Используется при сохранении итогового файла.
    // По использованию похоже что внутри там что-то типа версии итогово окошка с конфликтами но в виде данных.
    MergeLineList m_mergeLineList;
};








#endif // KDIFF3_MERGEDATAOBJ_H
