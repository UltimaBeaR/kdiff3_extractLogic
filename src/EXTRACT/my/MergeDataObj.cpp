#include "EXTRACT/my/MergeDataObj.h"

#include "EXTRACT/2_final/defmac.h"
#include "EXTRACT/2_final/options.h"
#include "EXTRACT/2_final/utils/Utils.h" // for Utils
#include "EXTRACT/0_consider/ui/KDiff3App_kdiff3.h"
#include "RLPainter.h"
#include "guiutils.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QCursor>
#include <QFile>
#include <QHBoxLayout>
#include <QtMath>
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

#include <KActionCollection>
#include <KLocalizedString>
#include <KMessageBox>







MergeDataObj::MergeDataObj()
{
}