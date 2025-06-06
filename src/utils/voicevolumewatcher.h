// Copyright (C) 2020 ~ 2021 Uniontech Software Technology Co.,Ltd.
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef VOICEVOLUMEWATCHER_H
#define VOICEVOLUMEWATCHER_H

#include <QTimer>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include "../dbus/com_deepin_daemon_audio.h"
#include "../dbus/com_deepin_daemon_audio_source.h"
#endif

#include "voicevolumewatcher_interface.h"
#include "audioutils.h"

// audio service monitor
class voiceVolumeWatcher : public voicevolumewatcher_interface
{
    Q_OBJECT

public:
    explicit voiceVolumeWatcher(QObject *parent = nullptr);
    // 新的构造函数，用于传递 DBus 连接信息给基类
    explicit voiceVolumeWatcher(const QString &service, const QString &path, 
                              const QDBusConnection &connection, QObject *parent = nullptr);
    ~voiceVolumeWatcher();

    // monitor audio change
    void setWatch(const bool isWatcher);
    // returns whether there is a system sound card or not
    bool getystemAudioState();

public Q_SLOTS:
    // changed the original run() method to a timer for quick exit
    void slotVoiceVolumeWatcher();

Q_SIGNALS:
    void sigRecodeState(bool couldUse);

protected:
    void onCardsChanged(const QString &value);
    void initAvailInputPorts(const QString &cards);
    bool isMicrophoneAvail(const QString &activePort) const;

    // For V20 or older
    void initV20DeviceWatcher();

private:
    struct Port
    {
        QString portId;
        QString portName;
        QString cardName;
        int available{1};
        int cardId;
        bool isActive{false};

        bool isInputPort() const;
        bool isLoopback() const;
    };
    friend QDebug &operator<<(QDebug &out, const Port &port);

    // For v23 or later
    AudioUtils *m_audioUtils{nullptr};

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    // For V20 or older
    // Audio interface com.deepin.daemon.Audio
    QScopedPointer<com::deepin::daemon::Audio> m_audioInterface;
    // Audio Source (Input Source)
    QScopedPointer<com::deepin::daemon::AudioSource::Source> m_defaultSource;
#endif

    // All available input ports.except loopback port.
    QMap<QString, Port> m_availableInputPorts;
    bool m_coulduse{false};

    QTimer *m_watchTimer{nullptr};     // microphone detection timer
    bool m_isExistSystemAudio{false};  // system sound card exists ? false by default

    bool m_version2{false};  // Use version 2.0 if system edition greater or equal V23
};

#endif  // VOICEVOLUMEWATCHER_H
