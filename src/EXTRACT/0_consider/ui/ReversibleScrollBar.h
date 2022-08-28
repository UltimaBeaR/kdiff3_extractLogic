#ifndef REVERSIBLESCROLLBAR_H
#define REVERSIBLESCROLLBAR_H

#include "EXTRACT/2_final/defmac.h"

#include <QApplication>
#include <QScrollBar>

class ReversibleScrollBar : public QScrollBar
{
    Q_OBJECT
    bool* m_pbRightToLeftLanguage;
    int m_realVal;

  public:
    ReversibleScrollBar(Qt::Orientation o, bool* pbRightToLeftLanguage)
        : QScrollBar(o)
    {
        m_pbRightToLeftLanguage = pbRightToLeftLanguage;
        m_realVal = 0;
        chk_connect(this, &ReversibleScrollBar::valueChanged, this, &ReversibleScrollBar::slotValueChanged);
    }
    void setAgain() { setValue(m_realVal); }

    void setValue(int i)
    {
        if(m_pbRightToLeftLanguage != nullptr && *m_pbRightToLeftLanguage)
            QScrollBar::setValue(maximum() - (i - minimum()));
        else
            QScrollBar::setValue(i);
    }

    int value() const
    {
        return m_realVal;
    }
  public Q_SLOTS:
    void slotValueChanged(int i)
    {
        m_realVal = i;
        if(m_pbRightToLeftLanguage != nullptr && *m_pbRightToLeftLanguage)
            m_realVal = maximum() - (i - minimum());
        Q_EMIT valueChanged2(m_realVal);
    }

  Q_SIGNALS:
    void valueChanged2(int);
};

#endif // REVERSIBLESCROLLBAR_H
