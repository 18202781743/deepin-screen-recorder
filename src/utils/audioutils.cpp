// Copyright (C) 2020 ~ 2021 Uniontech Software Technology Co.,Ltd.
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "audioutils.h"
#include "log.h"

#include <QDBusObjectPath>
#include <QDBusConnection>
#include <QDBusReply>
#include <QDBusInterface>
#include <QDBusError>
#include <QDBusMessage>
#include <QRegularExpression>

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include "../dbus/com_deepin_daemon_audio.h"
#include "../dbus/com_deepin_daemon_audio_sink.h"
#include "../dbus/com_deepin_daemon_audio_source.h"
#endif

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include "utils.h"

AudioUtils::AudioUtils(QObject *parent)
{
    Q_UNUSED(parent);

#if QT_VERISON >= QT_VERSION_CHECK(6, 0, 0)
    registerAudioPortMetaType();
#endif

    if (Utils::isSysGreatEqualV23()) {
        initAudioDBusInterface();
    }
}

// 初始化音频dbus服务的接口
void AudioUtils::initAudioDBusInterface()
{
    qCDebug(dsrApp) << "Initializing audio DBus interface";
    m_audioDBusInterface = new QDBusInterface(AudioService, AudioPath, AudioInterface, QDBusConnection::sessionBus());
    // 检查是否存在音频服务
    if (m_audioDBusInterface->isValid()) {
        qInfo() << "音频服务初始化成功！音频服务： " << AudioService << " 地址：" << AudioPath << " 接口：" << AudioInterface;
        m_defaultSourcePath = m_audioDBusInterface->property("DefaultSource").value<QDBusObjectPath>().path();
        initDefaultSourceDBusInterface();
        m_defaultSinkPath = m_audioDBusInterface->property("DefaultSink").value<QDBusObjectPath>().path();
        initDefaultSinkDBusInterface();
        // qDebug() << "sinks: " << m_audioDBusInterface->property("Sinks").value<QList<QDBusObjectPath>>();

        initConnections();
    } else {
        qWarning() << "初始化失败！音频服务 (" << AudioService << ") 不存在";
    }
}

// 初始化音频dbus服务默认输入源的接口
void AudioUtils::initDefaultSourceDBusInterface()
{
    qCDebug(dsrApp) << "Initializing default source DBus interface";
    if (m_defaultSourceDBusInterface) {
        delete m_defaultSourceDBusInterface;
        m_defaultSourceDBusInterface = nullptr;
    }
    // 初始化Dus接口
    m_defaultSourceDBusInterface =
        new QDBusInterface(AudioService, m_defaultSourcePath, SourceInterface, QDBusConnection::sessionBus());
    if (m_defaultSourceDBusInterface->isValid()) {
        qInfo() << "默认音频输入源初始化成功！音频服务： " << AudioService << " 默认输入源地址" << m_defaultSourcePath
                << " 默认输入源接口：" << SourceInterface;
    } else {
        qWarning() << "默认音频输入源初始化失败！默认输入源地址 (" << m_defaultSourcePath << ") 不存在";
    }
}

// 初始化音频dbus服务默认输出源的接口
void AudioUtils::initDefaultSinkDBusInterface()
{
    qCDebug(dsrApp) << "Initializing default sink DBus interface";
    if (m_defaultSinkDBusInterface) {
        delete m_defaultSinkDBusInterface;
        m_defaultSinkDBusInterface = nullptr;
    }
    // 初始化Dus接口
    m_defaultSinkDBusInterface =
        new QDBusInterface(AudioService, m_defaultSinkPath, SinkInterface, QDBusConnection::sessionBus());
    if (m_defaultSinkDBusInterface->isValid()) {
        qCInfo(dsrApp) << "Default audio sink initialized - Service:" << AudioService << "Path:" << m_defaultSinkPath
                << " Interface:" << SinkInterface;
    } else {
        qCWarning(dsrApp) << "Failed to initialize default audio sink at path:" << m_defaultSinkPath;
    }
}

// 初始化音频dbus服务属性改变链接
void AudioUtils::initConnections()
{
    qCDebug(dsrApp) << "Setting up audio DBus connections";
    // 监听音频服务的属性改变
    QDBusConnection::sessionBus().connect(AudioService,
                                          AudioPath,
                                          PropertiesInterface,
                                          "PropertiesChanged",
                                          "sa{sv}as",
                                          this,
                                          SLOT(onDBusAudioPropertyChanged(QDBusMessage)));
    qCDebug(dsrApp) << "Audio DBus connections established";
}

// 获取当前系统音频通道
QString AudioUtils::currentAudioChannel()
{
    qCDebug(dsrApp) << "Getting current audio channel";
    if (!Utils::isSysGreatEqualV23()) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        return currentAudioChannelV20Impl();
#endif
    }

    QString str_output = "-1";
    if (m_defaultSinkDBusInterface && m_defaultSinkDBusInterface->isValid()) {
        QString sinkName = m_defaultSinkDBusInterface->property("Name").toString();
        qDebug() << "系统声卡名称: " << sinkName;
        QStringList options;
        options << "-c";
        options << QString("pacmd list-sources | grep -PB 1 %1 | head -n 1 | perl -pe 's/.* //g'").arg(sinkName);
        QProcess process;
        process.start("bash", options);
        process.waitForFinished();
        process.waitForReadyRead();
        str_output = process.readAllStandardOutput();
        qCDebug(dsrApp) << "Audio channel from pacmd command:" << str_output;
        qCDebug(dsrApp) << "pacmd command: " << options;
        qCDebug(dsrApp) << "通过pacmd命令获取的系统音频通道号: " << str_output;
        if (str_output.isEmpty()) {
            QStringList options1;
            options1 << "-c";
            options1 << QString("pactl list sources | grep -PB 2 %1 | head -n 1 | perl -pe 's/.* #//g'").arg(sinkName);
            process.start("bash", options1);
            process.waitForFinished();
            process.waitForReadyRead();
            str_output = process.readAllStandardOutput().trimmed();
            qCDebug(dsrApp) << "Audio channel from pactl command:" << str_output;
            qCDebug(dsrApp) << "pactl command: " << options;
            qCDebug(dsrApp) << "通过pactl命令获取的系统音频通道号: " << str_output;
            if (str_output.isEmpty()) {
                if (!m_defaultSinkPath.isEmpty()) {
                    str_output = m_defaultSinkPath.right(1);
                    qCInfo(dsrApp) << "Using auto-assigned channel number:" << str_output;
                } else {
                    str_output = "-1";
                    qCWarning(dsrApp) << "Failed to auto-assign channel number, default sink path is empty";
                }
            }
        } else {
            if (str_output != "-1" && str_output.size() > 1) {
                str_output = str_output.left(str_output.size() - 1);
            }
        }
        return str_output;
    } else {
        str_output = "-1";
        qCWarning(dsrApp) << __FUNCTION__ << __LINE__ << "获取系统音频通道号失败！m_defaultSinkDBusInterface is nullptr or invalid ";
    }
    return str_output;
}

// 获取默认输出或输入设备名称
QString AudioUtils::getDefaultDeviceName(DefaultAudioType mode)
{
    qCDebug(dsrApp) << "Getting default device name for mode:" << mode;
    if (!Utils::isSysGreatEqualV23()) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        return getDefaultDeviceNameV20Impl(mode);
#endif
    }

    QString device = "";
    if (mode == DefaultAudioType::Sink) {
        // 1.首先取出默认系统声卡
        if (m_defaultSinkDBusInterface && m_defaultSinkDBusInterface->isValid()) {
            device = m_defaultSinkDBusInterface->property("Name").toString();
            qCDebug(dsrApp) << "Default sink name:" << device;
            if (!device.isEmpty() && !device.endsWith(".monitor")) {
                device += ".monitor";
            }
        } else {
            qCWarning(dsrApp) << "DBus Interface for audio sink is invalid";
        }
        // 2.如果默认系统声卡不是物理声卡和蓝牙声卡，需找出真实的物理声卡
        if (!device.startsWith("alsa", Qt::CaseInsensitive) && !device.startsWith("blue", Qt::CaseInsensitive)) {
            if (m_audioDBusInterface && m_audioDBusInterface->isValid()) {
                QList<QDBusObjectPath> sinks = m_audioDBusInterface->property("Sinks").value<QList<QDBusObjectPath>>();
                qCDebug(dsrApp) << "Searching for physical sound card in" << sinks.size() << "sinks";
                for (int i = 0; i < sinks.size(); i++) {
                    QDBusInterface *realSink =
                        new QDBusInterface(AudioService, sinks[i].path(), SinkInterface, QDBusConnection::sessionBus());
                    if (realSink->isValid()) {
                        device = realSink->property("Name").toString();
                        qCDebug(dsrApp) << "Found sink device:" << device;
                        if (device.startsWith("alsa", Qt::CaseInsensitive)) {
                            device += ".monitor";
                            break;
                        } else {
                            device = "";
                        }
                    }
                }
            } else {
                qCDebug(dsrApp) << __FUNCTION__ << __LINE__ << "m_audioDBusInterface is nullptr or invalid";
            }
        }
    } else if (mode == DefaultAudioType::Source) {
        if (m_defaultSourceDBusInterface && m_defaultSourceDBusInterface->isValid()) {
            device = m_defaultSourceDBusInterface->property("Name").toString();
            qCDebug(dsrApp) << "Default source name is : " << device;
            if (device.endsWith(".monitor")) {
                device.clear();
            }
        } else {
            qCWarning(dsrApp) << "DBus Interface(com.deepin.daemon.Auido.Source) is invalid!";
        }
    } else {
        qCDebug(dsrApp) << "The passed parameter is incorrect! Please pass in 1 or 2!";
    }
    return device;
}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
QString AudioUtils::currentAudioChannelV20Impl()
{
    const QString serviceName{"com.deepin.daemon.Audio"};

    QScopedPointer<com::deepin::daemon::Audio::Audio> audioInterface;
    QScopedPointer<com::deepin::daemon::AudioSink::Sink> defaultSink;

    audioInterface.reset(
        new com::deepin::daemon::Audio::Audio(serviceName, "/com/deepin/daemon/Audio", QDBusConnection::sessionBus(), this));

    defaultSink.reset(new com::deepin::daemon::AudioSink::Sink(
        serviceName, audioInterface->defaultSink().path(), QDBusConnection::sessionBus(), this));
    if (defaultSink->isValid()) {
        QString sinkName = defaultSink->name();
        QString command = QString("pacmd list-sources");

        QProcess process;
        process.start(command);
        process.waitForFinished();
        process.waitForReadyRead();
        QString str_output = process.readAllStandardOutput();

        QStringList lines = str_output.split("\n");
        QString targetLine;
        for (int i = 0; i < lines.size(); i++) {
            if (lines[i].contains(sinkName)) {
                if (i > 0) {
                    targetLine = lines[i - 1];
                    break;
                }
            }
        }

        if (!targetLine.isEmpty()) {
            targetLine.remove(QRegularExpression(".* "));
        }

        qCDebug(dsrApp) << command << targetLine;
        return targetLine;
    }
    return "";
}

QString AudioUtils::getDefaultDeviceNameV20Impl(DefaultAudioType mode)
{
    QString device = "";
    const QString serviceName{"com.deepin.daemon.Audio"};

    QScopedPointer<com::deepin::daemon::Audio> audioInterface;
    audioInterface.reset(
        new com::deepin::daemon::Audio(serviceName, "/com/deepin/daemon/Audio", QDBusConnection::sessionBus(), this));
    if (!audioInterface->isValid()) {
        qCWarning(dsrApp) << "DBus Interface(com.deepin.daemon.Auido) is invalid!";
        return device;
    }
    if (mode == DefaultAudioType::Sink) {
        // 1.首先取出默认系统声卡
        QScopedPointer<com::deepin::daemon::AudioSink::Sink> defaultSink;
        defaultSink.reset(new com::deepin::daemon::AudioSink::Sink(
            serviceName, audioInterface->defaultSink().path(), QDBusConnection::sessionBus(), this));
        if (defaultSink->isValid()) {
            qCInfo(dsrApp) << "Default sink name is : " << defaultSink->name();
            qCInfo(dsrApp) << "Default sink activePort name : " << defaultSink->activePort().name;
            qCInfo(dsrApp) << "             activePort description : " << defaultSink->activePort().description;
            qCInfo(dsrApp) << "             activePort availability : " << defaultSink->activePort().availability;
            device = defaultSink->name();
            if (!device.isEmpty() && !device.endsWith(".monitor")) {
                device += ".monitor";
            }
        } else {
            qCWarning(dsrApp) << "DBus Interface(com.deepin.daemon.Auido.Sink) is invalid!";
        }
        // 2.如果默认系统声卡不是物理声卡和蓝牙声卡，需找出真实的物理声卡
        if (!device.startsWith("alsa", Qt::CaseInsensitive) && !device.startsWith("blue", Qt::CaseInsensitive)) {
            QList<QScopedPointer<com::deepin::daemon::AudioSink::Sink>> sinks;
            QScopedPointer<com::deepin::daemon::AudioSink::Sink> realSink;
            for (int i = 0; i < audioInterface->sinks().size(); i++) {
                realSink.reset(new com::deepin::daemon::AudioSink::Sink(
                    serviceName, audioInterface->sinks()[i].path(), QDBusConnection::sessionBus(), this));
                if (realSink->isValid()) {
                    qCInfo(dsrApp) << "RealSink name is : " << realSink->name();
                    qCInfo(dsrApp) << "RealSink activePort name : " << realSink->activePort().name;
                    qCInfo(dsrApp) << "             activePort description : " << realSink->activePort().description;
                    qCInfo(dsrApp) << "             activePort availability : " << realSink->activePort().availability;
                    device = realSink->name();
                    if (device.startsWith("alsa", Qt::CaseInsensitive)) {
                        device += ".monitor";
                        break;
                    } else {
                        device = "";
                    }
                }
            }
        }
    } else if (mode == DefaultAudioType::Source) {
        QScopedPointer<com::deepin::daemon::AudioSource::Source> defaultSource;
        defaultSource.reset(new com::deepin::daemon::AudioSource::Source(
            serviceName, audioInterface->defaultSource().path(), QDBusConnection::sessionBus(), this));
        if (defaultSource->isValid()) {
            qCInfo(dsrApp) << "Default source name is : " << defaultSource->name();
            qCInfo(dsrApp) << "Default source activePort name : " << defaultSource->activePort().name;
            qCInfo(dsrApp) << "               activePort description : " << defaultSource->activePort().description;
            qCInfo(dsrApp) << "               activePort availability : " << defaultSource->activePort().availability;
            device = defaultSource->name();
            if (device.endsWith(".monitor")) {
                device.clear();
            }
        } else {
            qCWarning(dsrApp) << "DBus Interface(com.deepin.daemon.Auido.Source) is invalid!";
        }
    } else {
        qCDebug(dsrApp) << "The passed parameter is incorrect! Please pass in 1 or 2!";
    }
    return device;
}
#endif

// 音频dbus服务的接口
QDBusInterface *AudioUtils::audioDBusInterface()
{
    if (m_audioDBusInterface && m_audioDBusInterface->isValid()) {
        return m_audioDBusInterface;
    } else {
        qCDebug(dsrApp) << __FUNCTION__ << __LINE__ << "m_audioDBusInterface is nullptr or invalid";
        return nullptr;
    }
}

// 音频dbus服务默认输入源的接口
QDBusInterface *AudioUtils::defaultSourceDBusInterface()
{
    if (m_defaultSourceDBusInterface && m_defaultSourceDBusInterface->isValid()) {
        return m_defaultSourceDBusInterface;
    } else {
        qCDebug(dsrApp) << __FUNCTION__ << __LINE__ << "m_defaultSourceDBusInterface is nullptr or invalid";
        return nullptr;
    }
}

// 音频dbus服务默认输出源的接口
QDBusInterface *AudioUtils::defaultSinkDBusInterface()
{
    if (m_defaultSinkDBusInterface && m_defaultSinkDBusInterface->isValid()) {
        return m_defaultSinkDBusInterface;
    } else {
        qCDebug(dsrApp) << __FUNCTION__ << __LINE__ << "m_defaultSinkDBusInterface is nullptr or invalid";
        return nullptr;
    }
}

// 音频dbus服务默认输入源的活跃端口
AudioPort AudioUtils::defaultSourceActivePort()
{
    AudioPort port;
    auto inter = new QDBusInterface(AudioService, m_defaultSourcePath, PropertiesInterface, QDBusConnection::sessionBus());

    if (inter->isValid()) {
        // qInfo() << "音频服务： "<< AudioService <<" 默认输入源地址"<< m_defaultSourcePath << " 属性接口："<<
        // PropertiesInterface;
        QDBusReply<QDBusVariant> reply = inter->call("Get", SourceInterface, "ActivePort");
        reply.value().variant().value<QDBusArgument>() >> port;
        // qInfo() << "ActivePort:" << port;
    } else {
        qCDebug(dsrApp) << "默认输入源地址 (" << m_defaultSourcePath << ") 不存在";
    }
    delete inter;
    inter = nullptr;
    return port;
}

// 音频dbus服务默认输入源的音量
double AudioUtils::defaultSourceVolume()
{
    if (m_defaultSourceDBusInterface && m_defaultSourceDBusInterface->isValid()) {
        return m_defaultSourceDBusInterface->property("Volume").value<double>();
    } else {
        qCDebug(dsrApp) << __FUNCTION__ << __LINE__ << "m_defaultSourceDBusInterface is nullptr or invalid";
        return 0.0;
    }
}

// 音频dbus服务的声卡信息
QString AudioUtils::cards()
{
    if (m_audioDBusInterface && m_audioDBusInterface->isValid()) {
        return m_audioDBusInterface->property("Cards").toString();
    } else {
        qCDebug(dsrApp) << __FUNCTION__ << __LINE__ << "m_audioDBusInterface is nullptr or invalid";
        return "";
    }
}

// 音频dbus服务属性改变时触发
void AudioUtils::onDBusAudioPropertyChanged(QDBusMessage msg)
{
    QList<QVariant> arguments = msg.arguments();
    // qDebug() << "arguments" << arguments;
    // 参数固定长度
    if (3 != arguments.count()) {
        qCWarning(dsrApp) << "参数长度不为3！ 参数: " << arguments;
        return;
    }
    QString interfaceName = msg.arguments().at(0).toString();
    if (interfaceName == AudioInterface) {
        QVariantMap changedProps = qdbus_cast<QVariantMap>(arguments.at(1).value<QDBusArgument>());
        QStringList keys = changedProps.keys();
        foreach (const QString &prop, keys) {
            // qDebug() << "property: " << prop << changedProps[prop];
            if (prop == QStringLiteral("DefaultSource")) {
                // 默认输入源地址改变
                const QDBusObjectPath &defaultSourcePath = qvariant_cast<QDBusObjectPath>(changedProps[prop]);
                if (m_defaultSourcePath != defaultSourcePath.path()) {
                    qCInfo(dsrApp) << "默认输入源地址改变:" << m_defaultSourcePath << " To " << defaultSourcePath.path();
                    m_defaultSourcePath = defaultSourcePath.path();
                    // 发射默认输入源信号
                    emit defaultSourceChanaged();
                    // 重新初始化默认输入源接口
                    initDefaultSourceDBusInterface();
                }
            } else if (prop == QStringLiteral("Cards")) {
                // 声卡信息改变
                const QString &Cards = qvariant_cast<QString>(changedProps[prop]);
                if (m_audioCards != Cards) {
                    qCInfo(dsrApp) << "声卡信息改变:" << m_audioCards << " To " << Cards;
                    m_audioCards = Cards;
                    emit cardsChanged(m_audioCards);
                }
            }
        }
    }
}
