#include "MainDataObj.h"

// Эти инклюды взял с KDiff3App - взял смесь двух cpp файлов в реализациях (не факт что они все нужны) просто чтобы потом не париться при реализации.
// Потом в итоге нужно будет поубирать ненужное
// --------------------------------------------------------------------

#include "DirectoryInfo.h"
#include "EXTRACT/2_final/Logging.h"
#include "EXTRACT/0_consider/ui/MergeResultWindow.h"
#include "EXTRACT/my/MergeDataObj.h"
#include "EXTRACT/2_final/file_access/fileaccess.h"
#include "EXTRACT/2_final/defmac.h"
#include "EXTRACT/2_final/ui/progress.h"
#include "EXTRACT/2_final/utils/Utils.h"
#include "RLPainter.h"
#include "WindowTitleWidget.h"
#include "difftextwindow.h"
#include "directorymergewindow.h"
#include "guiutils.h"
#include "kdiff3_part.h"
#include "kdiff3_shell.h"
#include "optiondialog.h"
#include "smalldialogs.h"

#ifndef Q_OS_WIN
#include <unistd.h>
#endif

#include <algorithm>
#include <list>

// include files for QT
#include <QClipboard>
#include <QCheckBox>
#include <QCommandLineParser>
#include <QDesktopWidget>
#include <QDir>
#include <QFileDialog>
#include <QLayout>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QPaintDevice>
#include <QPainter>
#include <QPointer>
#include <QPrintDialog>
#include <QPrinter>
#include <QSplitter>
#include <QStatusBar>
#include <QTextEdit>
#include <QTextStream>
#include <QUrl>
// include files for KDE
#include <KCrash>
#include <KLocalizedString>
#include <KMessageBox>
#include <KStandardAction>
#include <KActionCollection>
#include <KToggleAction>
#include <KToolBar>
#include <QDialog>
#include <QEvent> // QKeyEvent, QDropEvent, QInputEvent
#include <QMimeData>
#include <QProcess>
#include <QScrollBar>
#include <QStringList>
#include <QTextCodec>
#include <KShortcutsDialog>

// --------------------------------------------------------------------

MainDataObj::MainDataObj()
{
}