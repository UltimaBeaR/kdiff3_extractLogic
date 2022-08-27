# SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
# SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
# SPDX-License-Identifier: GPL-2.0-or-later

########### kdiff3 KPart ###############
find_package(KF5 ${KF5_MIN_VERSION} REQUIRED COMPONENTS Parts WidgetsAddons)

set(
        kdiff3part_PART_SRCS
        kdiff3_part.cpp
        EXTRACT/0_consider/ui/KDiff3App_kdiff3.cpp
        DirectoryInfo.cpp
        directorymergewindow.cpp
        merger.cpp
        EXTRACT/0_consider/ui/KDiff3App_pdiff.cpp
        difftextwindow.cpp
        EXTRACT/2_final/diff.cpp
        optiondialog.cpp
        WindowTitleWidget.cpp
        EXTRACT/0_consider/ui/MergeResultWindow.cpp
        EXTRACT/my/MyOptions.cpp
        EXTRACT/my/MergeDataObj.cpp
        EXTRACT/2_final/file_access/fileaccess.cpp
        EXTRACT/2_final/file_access/DefaultFileAccessJobHandler.cpp
        EXTRACT/2_final/gnudiff/gnudiff_analyze.cpp
        EXTRACT/2_final/gnudiff/gnudiff_io.cpp
        EXTRACT/2_final/gnudiff/gnudiff_xmalloc.cpp
        EXTRACT/2_final/common.cpp
        smalldialogs.cpp
        EXTRACT/2_final/ui/progress.cpp
        EXTRACT/2_final/ui/ProgressProxyExtender.cpp
        PixMapUtils.cpp
        MergeFileInfos.cpp
        EXTRACT/2_final/utils/Utils.cpp
        EXTRACT/2_final/utils/Utils_urlToString.cpp
        selection.cpp
        EXTRACT/2_final/file_access/ignorelist.cpp
        EXTRACT/2_final/SourceData.cpp
        Overview.cpp
        EXTRACT/2_final/Logging.cpp
        FileNameLineEdit.cpp
        EXTRACT/2_final/MergeEditLine.cpp
        EXTRACT/2_final/Options.cpp
        EXTRACT/2_final/CommentParser.cpp
)

ki18n_wrap_ui(kdiff3part_PART_SRCS
        scroller.ui
        opendialog.ui
        )

add_library(kdiff3part MODULE ${kdiff3part_PART_SRCS})

set_target_properties(kdiff3part PROPERTIES DEFINE_SYMBOL KDIFF3_PART)
target_compile_features(kdiff3part PRIVATE ${needed_features})
target_link_libraries(kdiff3part ${KDiff3_LIBRARIES} KF5::Parts KF5::Crash)
target_compile_definitions(kdiff3part PRIVATE -DTRANSLATION_DOMAIN=\"kdiff3\")

install(TARGETS kdiff3part DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf5/parts)

########### kdiff3 executable ###############
find_package(KF5 ${KF5_MIN_VERSION} REQUIRED COMPONENTS Parts WidgetsAddons Config)

set(kdiff3_SRCS
        main.cpp
        kdiff3_shell.cpp
        ${kdiff3part_PART_SRCS})

#cann't use add_subdirectory because it changes the scope.
include(icons/CMakeLists.txt)
add_executable(kdiff3 ${kdiff3_SRCS})

target_link_libraries(kdiff3 KF5::ConfigCore KF5::ConfigGui KF5::Parts KF5::Crash ${KDiff3_LIBRARIES})
target_compile_features(kdiff3 PRIVATE ${needed_features})

# See https://cmake.org/cmake/help/v3.15/prop_tgt/MACOSX_BUNDLE_INFO_PLIST.html
if (APPLE)
    set_property(
            TARGET kdiff3
            PROPERTY MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/MacOSXBundleInfo.plist.in
    )

    # These are substituted by CMake into plist.in.
    set(MACOSX_BUNDLE_DISPLAY_NAME "KDiff3")
    set(MACOSX_BUNDLE_GUI_IDENTIFIER "org.kde.KDiff3")
    set(MACOSX_BUNDLE_BUNDLE_NAME "KDiff3")
    set(MACOSX_BUNDLE_DISPLAY_NAME "KDiff3")
    set(MACOSX_BUNDLE_INFO_STRING "KDiff3 - Diff/Patch Frontend")
    set(MACOSX_BUNDLE_LONG_VERSION_STRING "KDiff3 ${KDIFF3_VERSION}")
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${KDIFF3_VERSION_MAJOR}.${KDIFF3_VERSION_MINOR}")
    set(MACOSX_BUNDLE_BUNDLE_VERSION "${KDIFF3_VERSION}")
    set(MACOSX_BUNDLE_COPYRIGHT "2003-2020 The KDiff3 Authors")
endif ()

install(TARGETS kdiff3 ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})


########### install files ###############

install(FILES kdiff3_part.rc DESTINATION ${KDE_INSTALL_KXMLGUI5DIR}/kdiff3part)
install(FILES kdiff3_shell.rc DESTINATION ${KDE_INSTALL_KXMLGUI5DIR}/kdiff3)
#install( PROGRAMS  kdiff3.desktop DESTINATION ${XDG_APPS_INSTALL_DIR} )
install(PROGRAMS org.kde.kdiff3.desktop DESTINATION ${XDG_APPS_INSTALL_DIR})
install(FILES org.kde.kdiff3.appdata.xml DESTINATION ${KDE_INSTALL_METAINFODIR})

