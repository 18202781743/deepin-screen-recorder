// Copyright (C) 2020 ~ 2021 Uniontech Software Technology Co.,Ltd.
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "timewidget.h"
#include "dde-dock/constants.h"
#include "../../utils/log.h"

#include <DFontSizeManager>
#include <DGuiApplicationHelper>
#include <DStyle>
#include <DSysInfo>

#include <QApplication>
#include <QPainter>
#include <QDebug>
#include <QMouseEvent>
#include <QPixmap>
#include <QThread>
#include <QDBusInterface>
#include <QDBusPendingCall>
#include <QPainterPath>

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>

DCORE_USE_NAMESPACE

#define RECORDER_TIME_LEVEL_ICON_SIZE 23
#define RECORDER_TIME_VERTICAL_ICON_SIZE 16
#define RECORDER_TIME_FONT DFontSizeManager::instance()->t8()
#define RECORDER_PADDING 1
#define RECORDER_TIME_STARTCONFIG "CurrentStartTime"
#define RECORDER_TIME_STARTCOUNTCONFIG "CurrentStartCount"

TimeWidget::TimeWidget(DWidget *parent):
    DWidget(parent),
    m_timer(nullptr),
    m_dockInter(nullptr),
    m_lightIcon(nullptr),
    m_shadeIcon(nullptr),
    m_currentIcon(nullptr),
    m_bRefresh(true),
    m_position(Dock::Position::Bottom),
    m_hover(false),
    m_pressed(false),
    m_systemVersion(0),
    m_timerCount(0),
    m_setting(nullptr)
{
    setContentsMargins(0, 0, 0, 0);

    auto *layout = new QHBoxLayout(this);
    setLayout(layout);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSizeConstraint(QLayout::SetFixedSize);

    m_iconLabel = new QLabel(this);
    m_textLabel = new QLabel(this);
    layout->addWidget(m_iconLabel);
    layout->addWidget(m_textLabel);

    m_textLabel->setFont(RECORDER_TIME_FONT);
    m_textLabel->setText("00:00:00");

    auto updatePalette = [this](){
        qCDebug(dsrApp) << "Updating palette based on theme type:" << DGuiApplicationHelper::instance()->themeType();
        QPalette textPalette = m_textLabel->palette();
        if (DGuiApplicationHelper::instance()->themeType() == DGuiApplicationHelper::LightType) {
            textPalette.setColor(QPalette::WindowText, Qt::black);
        }else{
            textPalette.setColor(QPalette::WindowText, Qt::white);
        }
        m_textLabel->setPalette(textPalette);
    };
    updatePalette();
    connect(DGuiApplicationHelper::instance(), &DGuiApplicationHelper::themeTypeChanged, this, updatePalette);

    m_textLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_timer = new QTimer(this);
    m_dockInter = new timewidget_interface("org.deepin.dde.daemon.Dock1",
                                         "/org/deepin/dde/daemon/Dock1",
                                         QDBusConnection::sessionBus(),
                                         this);
    connect(m_dockInter, SIGNAL(PositionChanged(int)),
            this, SLOT(onPositionChanged(int)));
    
    m_position = m_dockInter->position();
    m_lightIcon = new QIcon(":/res/1070/light.svg");
    m_shadeIcon = new QIcon(":/res/1070/shade.svg");

    m_currentIcon = m_lightIcon;

    updateIcon();
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

    m_setting = new QSettings("deepin/deepin-screen-recorder", "recordtime", this);
}

TimeWidget::~TimeWidget()
{
    if (nullptr != m_lightIcon) {
        delete m_lightIcon;
        m_lightIcon = nullptr;
    }
    if (nullptr != m_shadeIcon) {
        delete m_shadeIcon;
        m_shadeIcon = nullptr;
    }
    if (nullptr != m_timer) {
        m_timer->deleteLater();
        m_timer = nullptr;
    }
    if (nullptr != m_dockInter) {
        m_dockInter->deleteLater();
        m_dockInter = nullptr;
    }
    if (nullptr != m_setting){
        m_setting->deleteLater();
        m_setting = nullptr;
    }
}

bool TimeWidget::enabled()
{
    return isEnabled();
}

void TimeWidget::onTimeout()
{
    m_timerCount++;
    if (m_bRefresh) {
        if (m_currentIcon == m_lightIcon)
            m_currentIcon = m_shadeIcon;
        else
            m_currentIcon = m_lightIcon;
    }
    m_bRefresh = !m_bRefresh;
    QTime showTime(0, 0, 0);
    showTime = showTime.addMSecs(m_timerCount * 400);
    m_setting->setValue(RECORDER_TIME_STARTCOUNTCONFIG, m_timerCount);
    m_textLabel->setText(showTime.toString("hh:mm:ss"));
    updateIcon();
}

void TimeWidget::updateIcon() {
    if (Dock::Position::Top == m_position || Dock::Position::Bottom == m_position) {
        m_pixmap = QIcon::fromTheme(QString("recordertime"), *m_currentIcon).pixmap(QSize(RECORDER_TIME_VERTICAL_ICON_SIZE, RECORDER_TIME_VERTICAL_ICON_SIZE));
    } else {
        m_pixmap = QIcon::fromTheme(QString("recordertime"), *m_currentIcon).pixmap(QSize(RECORDER_TIME_VERTICAL_ICON_SIZE, RECORDER_TIME_VERTICAL_ICON_SIZE));
    }

    m_iconLabel->setPixmap(m_pixmap);
}

void TimeWidget::onPositionChanged(int value)
{
    qInfo() << "====================== onPositionChanged ====start=================";
    m_position = value;

    if (m_position == Dock::Position::Top || m_position == Dock::Position::Bottom) {
        m_textLabel->show();
    } else {
        m_textLabel->hide();
    }

    qInfo() << "====================== onPositionChanged ====end=================";
}

QSettings *TimeWidget::setting() const
{
    return m_setting;
}

void TimeWidget::clearSetting()
{
    if (m_setting) {
        m_setting->setValue(RECORDER_TIME_STARTCONFIG, QTime(0,0,0));
        m_setting->setValue(RECORDER_TIME_STARTCOUNTCONFIG, 0);
    }
}

void TimeWidget::paintEvent(QPaintEvent *e)
{
    //qInfo() << "====================== paintEvent ====start=================";
    QPainter painter(this);
    //qInfo() <<  ">>>>>>>>>> rect().width(): " << rect().width() << " , this->height(): " << this->height();

    if (rect().height() > PLUGIN_BACKGROUND_MIN_SIZE) {
        QColor color;
        if (DGuiApplicationHelper::instance()->themeType() == DGuiApplicationHelper::LightType) {
            color = Qt::black;
            painter.setOpacity(0);
            if (m_hover) {
                painter.setOpacity(0.6);
            }

            if (m_pressed) {
                painter.setOpacity(0.3);
            }
        } else {
            color = Qt::white;
            painter.setOpacity(0);
            if (m_hover) {
                painter.setOpacity(0.2);
            }

            if (m_pressed) {
                painter.setOpacity(0.05);
            }
        }
        painter.setRenderHint(QPainter::Antialiasing, true);
        DStyleHelper dstyle(style());
        const int radius = dstyle.pixelMetric(DStyle::PM_FrameRadius);
        QPainterPath path;
        if ((Dock::Position::Top == m_position || Dock::Position::Bottom == m_position) ||
            ((Dock::Position::Right == m_position || Dock::Position::Left == m_position) && rect().width() > RECORDER_TIME_LEVEL_ICON_SIZE)) {
            QRect rc(RECORDER_PADDING, RECORDER_PADDING, rect().width() - RECORDER_PADDING * 2, rect().height() - RECORDER_PADDING * 2);
            path.addRoundedRect(rc, radius, radius);
        }
        painter.fillPath(path, color);
    }

    QWidget::paintEvent(e);
    //qInfo() << "====================== paintEvent ====end=================";
}

void TimeWidget::mousePressEvent(QMouseEvent *e)
{
    qDebug() << "Click the taskbar plugin! To start!";
    m_pressed = true;
    repaint();
    QWidget::mousePressEvent(e);
    qDebug() << "Click the taskbar plugin! The end!";
}
//创建缓存文件，只有wayland模式下的mips或部分arm架构适用
bool TimeWidget::createCacheFile()
{
    qDebug() << "createCacheFile start!";
    QString userName = QDir::homePath().section("/", -1, -1);
    std::string path = ("/home/" + userName + "/.cache/deepin/deepin-screen-recorder/").toStdString();
    QDir tdir(path.c_str());
    //判断文件夹路径是否存在
    if (!tdir.exists()) {
        tdir.mkpath(path.c_str());
    }
    path += "stopRecord.txt";
    //判断文件是否存在，若存在则先删除文件
    QFile mFile(path.c_str());
    if (mFile.exists()) {
        qCDebug(dsrApp) << "Removing existing cache file";
        remove(path.c_str());
    }
    //打开文件
    int fd = open(path.c_str(), O_RDWR | O_CREAT, 0644);
    if (fd == -1) {
        qCCritical(dsrApp) << "Failed to open cache file:" << strerror(errno);
        return false;
    }
    //文件加锁
    int flock = lockf(fd, F_TLOCK, 0);
    if (flock == -1) {
        qDebug() << "lock file fail!" << strerror(errno);
        return false;
    }
    ssize_t ret = -1;
    //文件内容为1，读取文件时会停止录屏
    char wBuffer[2] = {"1"};
    //写文件
    ret = write(fd, wBuffer, 2);
    if (ret < 0) {
        qCCritical(dsrApp) << "Failed to write to cache file";
        return false;
    }
    flock = lockf(fd, F_ULOCK, 0);
    if (flock == -1) {
        qCCritical(dsrApp) << "Failed to unlock cache file:" << strerror(errno);
        return false;
    }
    ::close(fd);
    qCInfo(dsrApp) << "Cache file created successfully";
    return true;
}

void TimeWidget::mouseReleaseEvent(QMouseEvent *e)
{
    qCDebug(dsrApp) << "Mouse release event received";
    if(e->button() == Qt::LeftButton && m_pressed && m_hover){
        m_pressed = false;
        m_hover = false;
        update();
        QWidget::mouseReleaseEvent(e);
        return;
    }
    int width = rect().width();
    bool flag = true;
    if (e->pos().x() > 0 && e->pos().x() < width) {
#if  defined (__mips__) || defined (__sw_64__) || defined (__loongarch_64__) || defined (__aarch64__) || defined (__loongarch__)
        if (isWaylandProtocol()) {
            flag = false;
            if(!createCacheFile()){
                qCWarning(dsrApp) << "Failed to create cache file for stopping recording";
                flag = true;
            };
        }
#endif
        if (flag) {
            qDebug() << "Click the taskbar plugin! Dbus call stop recording screen!";
            QDBusInterface notification(QString::fromUtf8("com.deepin.ScreenRecorder"),
                                        QString::fromUtf8("/com/deepin/ScreenRecorder"),
                                        QString::fromUtf8("com.deepin.ScreenRecorder"),
                                        QDBusConnection::sessionBus());
            notification.asyncCall("stopRecord");
            //        QDBusMessage message = notification.call("stopRecord"); //会阻塞任务其他按钮的执行
        }
    }
    m_pressed = false;
    m_hover = false;
    repaint();
    QWidget::mouseReleaseEvent(e);
    qDebug() << "Mouse release end!";

}

void TimeWidget::mouseMoveEvent(QMouseEvent *e)
{
    m_hover = true;
    repaint();
    QWidget::mouseMoveEvent(e);
}

void TimeWidget::leaveEvent(QEvent *e)
{
    m_hover = false;
    m_pressed = false;
    repaint();
    QWidget::leaveEvent(e);
}

void TimeWidget::start()
{
    qCInfo(dsrApp) << "Starting time widget";
    if (m_setting->value(RECORDER_TIME_STARTCONFIG).toTime() == QTime(0, 0, 0)) {
        qCDebug(dsrApp) << "Initializing start time";
        m_setting->setValue(RECORDER_TIME_STARTCONFIG, QTime::currentTime());
        m_baseTime = QTime::currentTime();
    } else {
        m_baseTime = m_setting->value(RECORDER_TIME_STARTCONFIG).toTime();
    }

    if (m_setting->value(RECORDER_TIME_STARTCOUNTCONFIG).toInt() == 0) {
        qCDebug(dsrApp) << "Initializing timer count";
        m_setting->setValue(RECORDER_TIME_STARTCOUNTCONFIG, 0);
        m_timerCount = 0;
    } else {
        m_timerCount = m_setting->value(RECORDER_TIME_STARTCOUNTCONFIG).toInt();
    }
    connect(m_timer, SIGNAL(timeout()), this, SLOT(onTimeout()));
    m_timer->start(400);
    qCDebug(dsrApp) << "Timer started with interval 400ms";
}

void TimeWidget::stop()
{
    qCInfo(dsrApp) << "Stopping time widget";
    disconnect(m_timer, SIGNAL(timeout()), this, SLOT(onTimeout()));
}

//判断是否是wayland协议
bool TimeWidget::isWaylandProtocol()
{
    QProcessEnvironment e = QProcessEnvironment::systemEnvironment();

    // check is treeland environment.
    if (e.value(QStringLiteral("DDE_CURRENT_COMPOSITOR")) == QStringLiteral("TreeLand")) {
        qCDebug(dsrApp) << "TreeLand environment detected";
        return false;
    }

    QString XDG_SESSION_TYPE = e.value(QStringLiteral("XDG_SESSION_TYPE"));
    QString WAYLAND_DISPLAY = e.value(QStringLiteral("WAYLAND_DISPLAY"));
    return XDG_SESSION_TYPE == QLatin1String("wayland") ||  WAYLAND_DISPLAY.contains(QLatin1String("wayland"), Qt::CaseInsensitive);
}

void TimeWidget::showEvent(QShowEvent *e)
{
    // 强制重新刷新 sizePolicy 和 size
    onPositionChanged(m_position);
    DWidget::showEvent(e);
}
