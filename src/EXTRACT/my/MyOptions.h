#ifndef KDIFF3_MYOPTIONS_H
#define KDIFF3_MYOPTIONS_H

#include "EXTRACT/2_final/options.h"

#include <QSharedPointer>

class MyOptions
{
  public:
    MyOptions(QSharedPointer<Options> pOptions);

    int getWhiteSpace2FileMergeDefault() { return m_pOptions->m_whiteSpace2FileMergeDefault; }
    int getWhiteSpace3FileMergeDefault() { return m_pOptions->m_whiteSpace3FileMergeDefault; }
  private:
    QSharedPointer<Options> m_pOptions;
};

#endif // KDIFF3_MYOPTIONS_H
