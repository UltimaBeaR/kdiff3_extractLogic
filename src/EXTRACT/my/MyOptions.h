#ifndef KDIFF3_MYOPTIONS_H
#define KDIFF3_MYOPTIONS_H

#include "EXTRACT/2_final/options.h"

#include <QSharedPointer>

class MyOptions
{
  public:
    MyOptions(QSharedPointer<Options> pOptions);

    // Временно, для мест где требуется что-то странное или во время рефакторинга
    const QSharedPointer<Options>& getOptions() { return  m_pOptions; }

    // --------------- MergeDataObj ----------------

    int whiteSpace2FileMergeDefault() { return m_pOptions->m_whiteSpace2FileMergeDefault; }
    int whiteSpace3FileMergeDefault() { return m_pOptions->m_whiteSpace3FileMergeDefault; }

    // --------------- SourceData -----------------

    // clear*** для команд - это во время чтения файлов вызывается. Хз зачем так, но оно так.

    QString preProcessorCmd() { return m_pOptions->m_PreProcessorCmd; }
    void clear_preProcessorCmd() { m_pOptions->m_PreProcessorCmd = ""; }

    QString lineMatchingPreProcessorCmd() { return m_pOptions->m_LineMatchingPreProcessorCmd; }
    void clear_lineMatchingPreProcessorCmd() { m_pOptions->m_LineMatchingPreProcessorCmd = ""; }

    bool ignoreComments() { return m_pOptions->m_bIgnoreComments; }
    bool ignoreCase() { return m_pOptions->m_bIgnoreCase; }

    QTextCodec* encodingPP() { return m_pOptions->m_pEncodingPP; }

    // -------------- MainDataObj --------------------

    bool autoSaveAndQuitOnMergeWithoutConflicts() { return m_pOptions->m_bAutoSaveAndQuitOnMergeWithoutConflicts; }

  private:
    QSharedPointer<Options> m_pOptions;
};

#endif // KDIFF3_MYOPTIONS_H
