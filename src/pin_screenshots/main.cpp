// Copyright (C) 2019 ~ 2019 Deepin Technology Co., Ltd.
// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "pinscreenshots.h"
#include "service/pinscreenshotsinterface.h"
#include "service/dbuspinscreenshotsadaptor.h"
#include "putils.h"
#include "../utils/log.h"

#include <DWidget>
#include <DLog>
#include <DWindowManagerHelper>
#include <DWidgetUtil>
#include <DGuiApplicationHelper>
#include <DApplication>

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusInterface>

#include <QScreen>
#include <QGuiApplication>

DWIDGET_USE_NAMESPACE

bool isWaylandProtocol()
{
    QProcessEnvironment e = QProcessEnvironment::systemEnvironment();

    // check is treeland environment.
    if (e.value(QStringLiteral("DDE_CURRENT_COMPOSITOR")) == QStringLiteral("TreeLand")) {
        return false;
    }

    QString XDG_SESSION_TYPE = e.value(QStringLiteral("XDG_SESSION_TYPE"));
    QString WAYLAND_DISPLAY = e.value(QStringLiteral("WAYLAND_DISPLAY"));
    return XDG_SESSION_TYPE == QLatin1String("wayland") ||  WAYLAND_DISPLAY.contains(QLatin1String("wayland"), Qt::CaseInsensitive);
}


int main(int argc, char *argv[])
{

    if (argc < 2) {
        qDebug() << "Cant open a null file";
        return 0;
    }
    PUtils::isWaylandMode = isWaylandProtocol();
    if (PUtils::isWaylandMode) {
        qCDebug(dsrApp) << "Setting Wayland shell integration";
        qputenv("QT_WAYLAND_SHELL_INTEGRATION", "kwayland-shell");
    }

#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    DGuiApplicationHelper::instance()->setPaletteType(DGuiApplicationHelper::UnknownType);
#else
    DGuiApplicationHelper::setUseInactiveColorGroup(false);
#endif

#if(DTK_VERSION < DTK_VERSION_CHECK(5,4,0,0))
    DApplication::loadDXcbPlugin();
    QScopedPointer<DApplication> app(new DApplication(argc, argv));
#else
    QScopedPointer<DApplication> app(DApplication::globalApplication(argc, argv));
#endif
    app->setOrganizationName("deepin");
    app->setApplicationName("deepin-screen-recorder");
    app->setProductName(QObject::tr("Pin Screenshots"));
    app->setApplicationVersion("1.0");
#if (QT_VERSION_MAJOR == 5)
    app->setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
    QString logFilePath = Dtk::Core::DLogManager::getlogFilePath();
    QStringList list = logFilePath.split("/");
    if (!list.isEmpty()) {
        list[list.count() - 1] = "deepin-pin-screen.log";
        logFilePath = list.join("/");
        Dtk::Core::DLogManager::setlogFilePath(logFilePath);
        qCInfo(dsrApp) << "Log file path set to:" << logFilePath;
    }

    Dtk::Core::DLogManager::registerConsoleAppender();
    Dtk::Core::DLogManager::registerFileAppender();

    QString logPath = Dtk::Core::DLogManager::getlogFilePath();
    logPath.replace("deepin-screen-recorder.log", "deepin-pin-screen.log");
    Dtk::Core::DLogManager::setlogFilePath(logPath);
    //qDebug() << "日志位置: " << Dtk::Core::DLogManager::getlogFilePath();
    QCommandLineOption dbusOption(QStringList() << "u" << "dbus", "Start  from dbus.");
    QCommandLineParser cmdParser;
    cmdParser.setApplicationDescription("deepin-pin-screenshots");
    cmdParser.addHelpOption();
    cmdParser.addVersionOption();
    cmdParser.addOption(dbusOption);
    cmdParser.process(*app);

    app->loadTranslator();

    qDebug() << "贴图日志路径: " << Dtk::Core::DLogManager::getlogFilePath();

    PinScreenShots instance;
    QDBusConnection dbus = QDBusConnection::sessionBus();

    if (dbus.registerService("com.deepin.PinScreenShots")) {
        // 第一次启动
        // 注册Dbus服务和对象
        dbus.registerObject("/com/deepin/PinScreenShots", &instance);
        // 初始化适配器
        new DbusPinScreenShotsAdaptor(&instance);
        qCInfo(dsrApp) << "DBus service registered successfully";

        if (cmdParser.isSet(dbusOption)) {
            // 第一次调用以 --dbus参数启动
            qDebug() << "dbus register waiting!";
            return app->exec();
        }

        instance.openFile(QString(argv[1]));
        qDebug() << "贴图dbus服务已注册";

    } else {
        // 第二次运行此应用，
        // 调用DBus接口，处理交给第一次调用的进程
        // 本进程退退出
        PinScreenShotsInterface *pinS = new PinScreenShotsInterface("com.deepin.PinScreenShots", "/com/deepin/PinScreenShots", QDBusConnection::sessionBus(), &instance);
        qDebug() << __FUNCTION__ << __LINE__;
        pinS->openFile(QString(argv[1]));
        delete pinS;
        return 0;
    }

    return app->exec();
}


