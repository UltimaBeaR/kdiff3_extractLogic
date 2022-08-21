/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
*/


#include "WindowTitleWidget.h"

#include "EXTRACT/0_consider/ui/KDiff3App_kdiff3.h"
#include "EXTRACT/2_final/options.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QCursor>
#include <QDir>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QPointer>
#include <QRegExp>
#include <QStatusBar>
#include <QTextCodec>
#include <QTextStream>

#include <KActionCollection>
#include <KLocalizedString>

WindowTitleWidget::WindowTitleWidget(const QSharedPointer<Options>& pOptions)
{
    m_pOptions = pOptions;
    setAutoFillBackground(true);

    QHBoxLayout* pHLayout = new QHBoxLayout(this);
    pHLayout->setContentsMargins(2, 2, 2, 2);
    pHLayout->setSpacing(2);

    m_pLabel = new QLabel(i18n("Output:"));
    pHLayout->addWidget(m_pLabel);

    m_pFileNameLineEdit = new FileNameLineEdit();
    pHLayout->addWidget(m_pFileNameLineEdit, 6);
    m_pFileNameLineEdit->installEventFilter(this);//for focus tracking
    m_pFileNameLineEdit->setAcceptDrops(true);
    m_pFileNameLineEdit->setReadOnly(true);

    //m_pBrowseButton = new QPushButton("...");
    //pHLayout->addWidget( m_pBrowseButton, 0 );
    //chk_connect( m_pBrowseButton, &QPushButton::clicked), this, &MergeResultWindow::slotBrowseButtonClicked);

    m_pModifiedLabel = new QLabel(i18n("[Modified]"));
    pHLayout->addWidget(m_pModifiedLabel);
    m_pModifiedLabel->setMinimumSize(m_pModifiedLabel->sizeHint());
    m_pModifiedLabel->setText("");

    pHLayout->addStretch(1);

    m_pEncodingLabel = new QLabel(i18n("Encoding for saving:"));
    pHLayout->addWidget(m_pEncodingLabel);

    m_pEncodingSelector = new QComboBox();
    m_pEncodingSelector->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    pHLayout->addWidget(m_pEncodingSelector, 2);
    setEncodings(nullptr, nullptr, nullptr);

    m_pLineEndStyleLabel = new QLabel(i18n("Line end style:"));
    pHLayout->addWidget(m_pLineEndStyleLabel);
    m_pLineEndStyleSelector = new QComboBox();
    m_pLineEndStyleSelector->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    pHLayout->addWidget(m_pLineEndStyleSelector);
    setLineEndStyles(eLineEndStyleUndefined, eLineEndStyleUndefined, eLineEndStyleUndefined);
}

void WindowTitleWidget::setFileName(const QString& fileName)
{
    m_pFileNameLineEdit->setText(QDir::toNativeSeparators(fileName));
}

QString WindowTitleWidget::getFileName()
{
    return m_pFileNameLineEdit->text();
}

//static QString getLineEndStyleName( e_LineEndStyle eLineEndStyle )
//{
//   if ( eLineEndStyle == eLineEndStyleDos )
//      return "DOS";
//   else if ( eLineEndStyle == eLineEndStyleUnix )
//      return "Unix";
//   return QString();
//}

void WindowTitleWidget::setLineEndStyles(e_LineEndStyle eLineEndStyleA, e_LineEndStyle eLineEndStyleB, e_LineEndStyle eLineEndStyleC)
{
    m_pLineEndStyleSelector->clear();
    QString dosUsers;
    if(eLineEndStyleA == eLineEndStyleDos)
        dosUsers += i18n("A");
    if(eLineEndStyleB == eLineEndStyleDos)
        dosUsers += QLatin1String(dosUsers.isEmpty() ? "" : ", ") + i18n("B");
    if(eLineEndStyleC == eLineEndStyleDos)
        dosUsers += QLatin1String(dosUsers.isEmpty() ? "" : ", ") + i18n("C");
    QString unxUsers;
    if(eLineEndStyleA == eLineEndStyleUnix)
        unxUsers += i18n("A");
    if(eLineEndStyleB == eLineEndStyleUnix)
        unxUsers += QLatin1String(unxUsers.isEmpty() ? "" : ", ") + i18n("B");
    if(eLineEndStyleC == eLineEndStyleUnix)
        unxUsers += QLatin1String(unxUsers.isEmpty() ? "" : ", ") + i18n("C");

    m_pLineEndStyleSelector->addItem(i18n("Unix") + (unxUsers.isEmpty() ? QString("") : QLatin1String(" (") + unxUsers + QLatin1String(")")));
    m_pLineEndStyleSelector->addItem(i18n("DOS") + (dosUsers.isEmpty() ? QString("") : QLatin1String(" (") + dosUsers + QLatin1String(")")));

    e_LineEndStyle autoChoice = (e_LineEndStyle)m_pOptions->m_lineEndStyle;

    if(m_pOptions->m_lineEndStyle == eLineEndStyleAutoDetect)
    {
        if(eLineEndStyleA != eLineEndStyleUndefined && eLineEndStyleB != eLineEndStyleUndefined && eLineEndStyleC != eLineEndStyleUndefined)
        {
            if(eLineEndStyleA == eLineEndStyleB)
                autoChoice = eLineEndStyleC;
            else if(eLineEndStyleA == eLineEndStyleC)
                autoChoice = eLineEndStyleB;
            else
                autoChoice = eLineEndStyleConflict; //conflict (not likely while only two values exist)
        }
        else
        {
            e_LineEndStyle c1, c2;
            if(eLineEndStyleA == eLineEndStyleUndefined)
            {
                c1 = eLineEndStyleB;
                c2 = eLineEndStyleC;
            }
            else if(eLineEndStyleB == eLineEndStyleUndefined)
            {
                c1 = eLineEndStyleA;
                c2 = eLineEndStyleC;
            }
            else /*if( eLineEndStyleC == eLineEndStyleUndefined )*/
            {
                c1 = eLineEndStyleA;
                c2 = eLineEndStyleB;
            }
            if(c1 == c2 && c1 != eLineEndStyleUndefined)
                autoChoice = c1;
            else
                autoChoice = eLineEndStyleConflict;
        }
    }

    if(autoChoice == eLineEndStyleUnix)
        m_pLineEndStyleSelector->setCurrentIndex(0);
    else if(autoChoice == eLineEndStyleDos)
        m_pLineEndStyleSelector->setCurrentIndex(1);
    else if(autoChoice == eLineEndStyleConflict)
    {
        m_pLineEndStyleSelector->addItem(i18n("Conflict"));
        m_pLineEndStyleSelector->setCurrentIndex(2);
    }
}

e_LineEndStyle WindowTitleWidget::getLineEndStyle()
{

    int current = m_pLineEndStyleSelector->currentIndex();
    if(current == 0)
        return eLineEndStyleUnix;
    else if(current == 1)
        return eLineEndStyleDos;
    else
        return eLineEndStyleConflict;
}

void WindowTitleWidget::setEncodings(QTextCodec* pCodecForA, QTextCodec* pCodecForB, QTextCodec* pCodecForC)
{
    m_pEncodingSelector->clear();

    // First sort codec names:
    std::map<QString, QTextCodec*> names;
    const QList<int> mibs = QTextCodec::availableMibs();
    for(int i: mibs)
    {
        QTextCodec* c = QTextCodec::codecForMib(i);
        if(c != nullptr)
            names[QLatin1String(c->name())] = c;
    }

    if(pCodecForA != nullptr)
        m_pEncodingSelector->addItem(i18n("Codec from A: %1", QLatin1String(pCodecForA->name())), QVariant::fromValue((void*)pCodecForA));
    if(pCodecForB != nullptr)
        m_pEncodingSelector->addItem(i18n("Codec from B: %1", QLatin1String(pCodecForB->name())), QVariant::fromValue((void*)pCodecForB));
    if(pCodecForC != nullptr)
        m_pEncodingSelector->addItem(i18n("Codec from C: %1", QLatin1String(pCodecForC->name())), QVariant::fromValue((void*)pCodecForC));

    std::map<QString, QTextCodec*>::const_iterator it;
    for(it = names.begin(); it != names.end(); ++it)
    {
        m_pEncodingSelector->addItem(it->first, QVariant::fromValue((void*)it->second));
    }
    m_pEncodingSelector->setMinimumSize(m_pEncodingSelector->sizeHint());

    if(pCodecForC != nullptr && pCodecForB != nullptr && pCodecForA != nullptr)
    {
        if(pCodecForA == pCodecForC)
            m_pEncodingSelector->setCurrentIndex(1); // B
        else
            m_pEncodingSelector->setCurrentIndex(2); // C
    }
    else if(pCodecForA != nullptr && pCodecForB != nullptr)
        m_pEncodingSelector->setCurrentIndex(1); // B
    else
        m_pEncodingSelector->setCurrentIndex(0);
}

QTextCodec* WindowTitleWidget::getEncoding()
{
    return (QTextCodec*)m_pEncodingSelector->itemData(m_pEncodingSelector->currentIndex()).value<void*>();
}

void WindowTitleWidget::setEncoding(QTextCodec* pEncoding)
{
    int idx = m_pEncodingSelector->findText(QLatin1String(pEncoding->name()));
    if(idx >= 0)
        m_pEncodingSelector->setCurrentIndex(idx);
}

//void WindowTitleWidget::slotBrowseButtonClicked()
//{
//   QString current = m_pFileNameLineEdit->text();
//
//   QUrl newURL = KFileDialog::getSaveUrl( current, 0, this, i18n("Select file (not saving yet)"));
//   if ( !newURL.isEmpty() )
//   {
//      m_pFileNameLineEdit->setText( newURL.url() );
//   }
//}

void WindowTitleWidget::slotSetModified(bool bModified)
{
    m_pModifiedLabel->setText(bModified ? i18n("[Modified]") : "");
}

bool WindowTitleWidget::eventFilter(QObject* o, QEvent* e)
{
    Q_UNUSED(o);
    if(e->type() == QEvent::FocusIn || e->type() == QEvent::FocusOut)
    {
        QPalette p = m_pLabel->palette();

        QColor c1 = m_pOptions->m_fgColor;
        QColor c2 = Qt::lightGray;
        if(e->type() == QEvent::FocusOut)
            c2 = m_pOptions->m_bgColor;

        p.setColor(QPalette::Window, c2);
        setPalette(p);

        p.setColor(QPalette::WindowText, c1);
        m_pLabel->setPalette(p);
        m_pEncodingLabel->setPalette(p);
        m_pEncodingSelector->setPalette(p);
    }
    return false;
}

//#include "mergeresultwindow.moc"
