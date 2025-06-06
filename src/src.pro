######################################################################
# Automatically generated by qmake (3.0) ?? 2? 4 17:49:37 2017
######################################################################
#兼容性 编译宏定义
# 1050 新添加Wayland相关库
# 103x 不支持滚动截图和ocr
# 102x DBus接口Server中不支持startdde相关接口
# /etc/os-version 依赖包deepin-desktop-base
SYS_INFO=$$system("cat /etc/os-version")
message("SYS_INFO: " $$SYS_INFO)

SYS_EDITION=$$system("cat /etc/os-version | grep 'Community'")
message("SYS_EDITION: " $$SYS_EDITION)

DEFINES += APP_VERSION=\\\"$$VERSION\\\"
message("APP_VERSION: " $$APP_VERSION)

SYS_MAJOR_VERSION=$$system("cat /etc/os-version | grep 'MajorVersion' | grep -o '\-\?[0-9]\+'")
message("SYS_MAJOR_VERSION: " $$SYS_MAJOR_VERSION)
SYS_VERSION=$$system("cat /etc/os-version | grep 'MinorVersion' | grep -o '\-\?[0-9]\+'")
message("SYS_VERSION: " $$SYS_VERSION)

#cat /etc/os-version 中的OsBuild
SYS_BUILD=$$system("cat /etc/os-version | grep 'OsBuild' | grep -o '[0-9]\+\.\?[0-9]\+'")
SYS_BUILD_PREFIX=$$system("cat /etc/os-version | grep 'OsBuild' | grep -o '[0-9]\+\.' | grep -o '[0-9]\+'")
SYS_BUILD_SUFFIX=$$system("cat /etc/os-version | grep 'OsBuild' | grep -o '\.[0-9]\+' | grep -o '[0-9]\+'")
message("SYS_BUILD: " $$SYS_BUILD)
message("SYS_BUILD_PREFIX: " $$SYS_BUILD_PREFIX)
#SYS_BUILD_SUFFIX 系统镜像批次号
message("SYS_BUILD_SUFFIX: " $$SYS_BUILD_SUFFIX)
warning("qtversion: " $$QT_MAJOR_VERSION)

# The latest version use XWayland temporarily.
# TODO: Adapt to the new version of Wayland interface or use PipeWire in the future
if ( !equals(SYS_EDITION, "") || equals(SYS_VERSION, 2500) || greaterThan(SYS_VERSION, 2500) ) {
    # Community edition or latest edition
    message("Community / 2500")

    DEFINES += DDE_START_FLAGE_ON
    message("startdde support: OK!!!")

    DEFINES += OCR_SCROLL_FLAGE_ON
    message("ocr and scroll support: OK!!!")

} else {
    message("not Community")

    #1030 支持startdde加速
    greaterThan(SYS_VERSION, 1029) {
        DEFINES += DDE_START_FLAGE_ON
        message("startdde support: OK!!!")
    }

    #1040 兼容支持ocr和滚动截图
    greaterThan(SYS_VERSION, 1039) {
        DEFINES += OCR_SCROLL_FLAGE_ON
        message("ocr and scroll support: OK!!!")
    }

    #1050支持Wayland
    greaterThan(SYS_VERSION, 1049) {
        DEFINES += KF5_WAYLAND_FLAGE_ON
        message("wayland support: OK!!!")
        #1054Wayland remote协议新增接口 109表示1053
        greaterThan(SYS_BUILD_SUFFIX, 109) {
            DEFINES += KWAYLAND_REMOTE_FLAGE_ON
            message("wayland remote support: OK!!!")
        }
        #1055Wayland remote协议新增release接口 110表示1054
        greaterThan(SYS_BUILD_SUFFIX, 110) {
            DEFINES += KWAYLAND_REMOTE_BUFFER_RELEASE_FLAGE_ON
            message("wayland remote support: OK!!!")
        }
        greaterThan(SYS_VERSION, 1059) {
            DEFINES += KWAYLAND_REMOTE_FLAGE_ON
            DEFINES += KWAYLAND_REMOTE_BUFFER_RELEASE_FLAGE_ON
            message("wayland remote buffer release support: OK!!!")
        }
    }
}

# 基础 Qt 模块配置
equals(QT_MAJOR_VERSION, 6) {
    QT += core gui widgets network dbus multimedia multimediawidgets concurrent svg \
          sql xml opengl openglwidgets waylandclient waylandclient-private
    
    # Qt6 specific configurations
    PKGCONFIG += dtk6widget dtk6core dtk6gui xcb xcb-util gstreamer-app-1.0 libusb-1.0
    
    QMAKE_LRELEASE = /usr/lib/qt6/bin/lrelease
    
    # Comment out KF6 libraries that are not found
    #LIBS += -L$$PREFIX/lib/$$QMAKE_HOST.arch-linux-gnu/ -lKF6GlobalAccel -lKF6I18n
    #INCLUDEPATH += $$PREFIX/include/KF6/KGlobalAccel
    #INCLUDEPATH += $$PREFIX/include/KF6/KI18n

    INCLUDEPATH += /usr/include/x86_64-linux-gnu/qt6/QtWidgets
    
} else {
    QT += core gui widgets network dbus multimedia multimediawidgets concurrent x11extras svg \
          sql xml
    
    # Qt5 specific configurations 
    PKGCONFIG += dtkwidget dtkcore dtkgui xcb xcb-util gstreamer-app-1.0 libusb-1.0
    
    QMAKE_LRELEASE = lrelease
    
    LIBS += -L$$PREFIX/lib/$$QMAKE_HOST.arch-linux-gnu/ -lKF5GlobalAccel -lKF5I18n
    INCLUDEPATH += $$PREFIX/include/KF5/KGlobalAccel
    INCLUDEPATH += $$PREFIX/include/KF5/KI18n
    
    # Comment out or remove Wayland support for Qt5
    # contains(DEFINES, KF5_WAYLAND_FLAGE_ON) {
    #     QT += KI18n KWaylandClient
    #     LIBS += -lepoxy
    #     
    #     if (greaterThan(SYS_MAJOR_VERSION, 20)) {
    #         QT += DWaylandClient
    #         DEFINES += DWAYLAND_SUPPORT
    #     }
    # }
}

TEMPLATE = app
TARGET = deepin-screen-recorder

# Include paths
INCLUDEPATH += . \
            /usr/include/gstreamer-1.0 \
            /usr/include/opencv_mobile

INCLUDEPATH += ../3rdparty/libcam/libcam/ \
               ../3rdparty/libcam/libcam_v4l2core/ \
               ../3rdparty/libcam/libcam_render/ \
               ../3rdparty/libcam/libcam_encoder/ \
               ../3rdparty/libcam/libcam_audio/ \
               ../3rdparty/libcam/load_libs.h \
               ../3rdparty/displayjack/wayland_client.h

message("PWD: " $$PWD)

# Include pri files
include(accessibility/accessible.pri)
include(../3rdparty/libcam/libcam.pri)


# Compiler flags
CONFIG += c++17 link_pkgconfig
QMAKE_CXXFLAGS += -g -Wall -Wno-error=deprecated-declarations -Wno-deprecated-declarations
QMAKE_LFLAGS += -g -Wall -Wl,--as-needed -pie -z noexecstack -fstack-protector-all -z now

# Architecture specific settings
ARCH = $$QMAKE_HOST.arch
isEqual(ARCH, mips64) {
    QMAKE_CXX += -O3 -ftree-vectorize -march=loongson3a -mhard-float -mno-micromips -mno-mips16 -flax-vector-conversions -mloongson-ext2 -mloongson-mmi
    QMAKE_CXXFLAGS += -O3 -ftree-vectorize -march=loongson3a -mhard-float -mno-micromips -mno-mips16 -flax-vector-conversions -mloongson-ext2 -mloongson-mmi
}

# Headers
HEADERS += main_window.h \
    capture.h \
    qwayland-treeland-capture-unstable-v1.h \
    record_process.h \
    utils.h \
    utils/log.h \
    countdown_tooltip.h \
    constant.h \
    event_monitor.h \
    button_feedback.h \
    show_buttons.h \
    keydefine.h   \
    menucontroller/menucontroller.h \
    utils/proxyaudioport.h \
    utils/voicevolumewatcher_interface.h \
    utils/x_multi_screen_info.h \
    utils_interface.h \
    voicevolumewatcherext.h \
    utils/baseutils.h \
    utils/configsettings.h \
    utils/shortcut.h \
    utils/tempfile.h \
    utils/calculaterect.h \
    utils/saveutils.h \
    utils/shapesutils.h \
    wayland-treeland-capture-unstable-v1-client-protocol.h \
    widgets/zoomIndicator.h \
    widgets/textedit.h \
    widgets/toptips.h \
    widgets/toolbar.h \
    widgets/shapeswidget.h \
    widgets/toolbutton.h \
    widgets/subtoolwidget.h \
    widgets/keybuttonwidget.h \
    widgets/sidebar.h \
    widgets/shottoolwidget.h \
    widgets/colortoolwidget.h \
    dbusinterface/dbusnotify.h \
    dbusservice/dbusscreenshotservice.h \
    widgets/camerawidget.h \
    screenshot.h \
    utils/voicevolumewatcher.h \
    utils/camerawatcher.h \
    widgets/tooltips.h \
    widgets/filter.h \
    utils/screengrabber.h \
    RecorderRegionShow.h \
    recordertablet.h \
    dbusinterface/ocrinterface.h \
    dbusinterface/pinscreenshotsinterface.h \
    widgets/imagemenu.h \
    utils/borderprocessinterface.h \
    widgets/zoomIndicatorGL.h \
    gstrecord/gstrecordx.h \
    utils/audioutils.h \
    gstrecord/gstinterface.h \
    camera/devnummonitor.h \
    camera/LPF_V4L2.h \
    camera/majorimageprocessingthread.h \
    utils/eventlogutils.h \
    widgets/slider.h

# Qt 版本特定的头文件
lessThan(QT_MAJOR_VERSION, 6) {
    # 仅在 Qt5 下包含的头文件
    HEADERS += \
        dbus/audioport.h \
        dbus/audioportlist.h \
        dbus/com_deepin_daemon_audio_source.h \
        dbus/com_deepin_daemon_audio.h \
        dbus/com_deepin_daemon_audio_sink.h
}

# Sources
SOURCES += main.cpp \
    capture.cpp \
    main_window.cpp \
    qwayland-treeland-capture-unstable-v1.cpp \
    record_process.cpp \
    utils.cpp \
    utils/log.cpp \
    countdown_tooltip.cpp \
    constant.cpp \
    event_monitor.cpp \
    button_feedback.cpp \
    show_buttons.cpp  \
    menucontroller/menucontroller.cpp \
    utils/proxyaudioport.cpp \
    utils/shapesutils.cpp \
    utils/tempfile.cpp \
    utils/calculaterect.cpp \
    utils/shortcut.cpp \
    utils/configsettings.cpp \
    utils/baseutils.cpp \
    utils/voicevolumewatcher_interface.cpp \
    utils/x_multi_screen_info.cpp \
    utils_interface.cpp \
    wayland-treeland-capture-unstable-v1-protocol.c \
    widgets/toptips.cpp \
    widgets/shapeswidget.cpp \
    widgets/textedit.cpp \
    widgets/zoomIndicator.cpp \
    widgets/toolbar.cpp \
    widgets/subtoolwidget.cpp \
    widgets/keybuttonwidget.cpp \
    widgets/sidebar.cpp \
    widgets/shottoolwidget.cpp \
    widgets/colortoolwidget.cpp \
    dbusinterface/dbusnotify.cpp \
    dbusservice/dbusscreenshotservice.cpp \
    widgets/camerawidget.cpp \
    screenshot.cpp \
    utils/voicevolumewatcher.cpp \
    utils/camerawatcher.cpp \
    widgets/tooltips.cpp \
    widgets/filter.cpp \
    utils/screengrabber.cpp \
    RecorderRegionShow.cpp \
    recordertablet.cpp \
    dbusinterface/ocrinterface.cpp \
    dbusinterface/pinscreenshotsinterface.cpp \
    widgets/imagemenu.cpp \
    utils/borderprocessinterface.cpp \
    widgets/zoomIndicatorGL.cpp \
    gstrecord/gstrecordx.cpp \
    utils/audioutils.cpp \
    gstrecord/gstinterface.cpp \
    camera/devnummonitor.cpp \
    camera/majorimageprocessingthread.cpp \
    camera/LPF_V4L2.c \
    utils/eventlogutils.cpp

# Qt 版本特定的源文件
lessThan(QT_MAJOR_VERSION, 6) {
    # 仅在 Qt5 下包含的源文件
    SOURCES += \
        dbus/audioport.cpp \
        dbus/audioportlist.cpp \
        dbus/com_deepin_daemon_audio_source.cpp \
        dbus/com_deepin_daemon_audio.cpp \
        dbus/com_deepin_daemon_audio_sink.cpp
}

# OCR and Scroll screenshot support
contains(DEFINES, OCR_SCROLL_FLAGE_ON) {
    HEADERS += widgets/scrollshottip.h \
        utils/pixmergethread.h \
        utils/scrollScreenshot.h \
        widgets/previewwidget.h

    SOURCES += widgets/scrollshottip.cpp \
        utils/pixmergethread.cpp \
        utils/scrollScreenshot.cpp \
        widgets/previewwidget.cpp
}

# Comment out or remove Wayland support section
# contains(DEFINES, KF5_WAYLAND_FLAGE_ON) {
#     HEADERS += waylandrecord/writeframethread.h \
#         waylandrecord/waylandintegration.h \
#         waylandrecord/waylandintegration_p.h \
#         waylandrecord/recordadmin.h \
#         waylandrecord/avoutputstream.h \
#         waylandrecord/avinputstream.h \
#         waylandrecord/avlibinterface.h \
#         utils/waylandmousesimulator.h \
#         utils/waylandscrollmonitor.h
# 
#     SOURCES += waylandrecord/writeframethread.cpp \
#         waylandrecord/waylandintegration.cpp \
#         waylandrecord/recordadmin.cpp \
#         waylandrecord/avinputstream.cpp \
#         waylandrecord/avoutputstream.cpp \
#         waylandrecord/avlibinterface.cpp \
#         utils/waylandmousesimulator.cpp \
#         utils/waylandscrollmonitor.cpp
# }

# Resources
RESOURCES = ../assets/image/deepin-screen-recorder.qrc \
    ../assets/icons/icons.qrc
    # ../resources.qrc

# Libraries
LIBS += -lX11 -lXext -lXtst -lXfixes -lXcursor -ldl -lXinerama

contains(DEFINES, OCR_SCROLL_FLAGE_ON) {
    LIBS += -lopencv_small
}

# Installation paths
isEmpty(PREFIX) {
    PREFIX = /usr
}

isEmpty(BINDIR):BINDIR=$$PREFIX/bin
isEmpty(ICONDIR):ICONDIR=$$PREFIX/share/icons/hicolor/scalable/apps
isEmpty(APPDIR):APPDIR=$$PREFIX/share/applications
isEmpty(DSRDIR):DSRDIR=$$PREFIX/share/deepin-screen-recorder
isEmpty(DOCDIR):DOCDIR=$$PREFIX/share/dman/deepin-screen-recorder
isEmpty(ETCDIR):ETCDIR=/etc
isEmpty(TABCONDIR):TABCONDIR=/etc/due-shell/json

# Installation targets
target.path = $$INSTROOT$$BINDIR
icon.path = $$INSTROOT$$ICONDIR
desktop.path = $$INSTROOT$$APPDIR
translations.path = $$INSTROOT$$DSRDIR/translations

icon.files = ../assets/image/deepin-screen-recorder.svg ../assets/image/deepin-screenshot.svg
desktop.files = ../deepin-screen-recorder.desktop

# DBus service
dbus_service.files = $$PWD/../assets/com.deepin.ScreenRecorder.service $$PWD/../assets/com.deepin.Screenshot.service
contains(DEFINES, DDE_START_FLAGE_ON) {
    dbus_service.files = $$PWD/../com.deepin.ScreenRecorder.service $$PWD/../com.deepin.Screenshot.service
}
dbus_service.path = $$PREFIX/share/dbus-1/services

# Manual
manual_dir.files = $$PWD/../assets/deepin-screen-recorder
manual_dir.path = $$PREFIX/share/deepin-manual/manual-assets/application/

INSTALLS += target icon desktop translations dbus_service manual_dir
