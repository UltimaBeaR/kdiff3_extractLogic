#ifndef INITFLAGS_H
#define INITFLAGS_H

#include <QApplication>

enum class InitFlag
{
    loadFiles = 1,
    useCurrentEncoding = 2,
    autoSolve = 4,
    initGUI = 8,
    defaultFlags = loadFiles | autoSolve | initGUI
};

Q_DECLARE_FLAGS(InitFlags, InitFlag);
Q_DECLARE_OPERATORS_FOR_FLAGS(InitFlags);

#endif // INITFLAGS_H
