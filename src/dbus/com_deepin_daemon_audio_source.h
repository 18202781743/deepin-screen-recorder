/*
 * This file was generated by qdbusxml2cpp version 0.8
 * Command line was: qdbusxml2cpp -p com_deepin_daemon_audio_source com.deepin.daemon.Audio.Source.xml
 *
 * qdbusxml2cpp is Copyright (C) 2020 The Qt Company Ltd.
 *
 * This is an auto-generated file.
 * Do not edit! All changes made to it will be lost.
 */

#ifndef COM_DEEPIN_DAEMON_AUDIO_SOURCE_H
#define COM_DEEPIN_DAEMON_AUDIO_SOURCE_H

#include <QtCore/QObject>
#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>
#include <QtDBus/QtDBus>
#include "audioport.h"
#include "audioportlist.h"
/*
 * Proxy class for interface com.deepin.daemon.Audio.Source
 */
class ComDeepinDaemonAudioSourceInterface: public QDBusAbstractInterface
{
    Q_OBJECT
public:
    static inline const char *staticInterfaceName()
    { return "com.deepin.daemon.Audio.Source"; }

public:
    ComDeepinDaemonAudioSourceInterface(const QString &service, const QString &path, const QDBusConnection &connection, QObject *parent = nullptr);

    ~ComDeepinDaemonAudioSourceInterface();

    Q_PROPERTY(AudioPort ActivePort READ activePort)
    inline AudioPort activePort() const
    { return qvariant_cast< AudioPort >(property("ActivePort")); }

    Q_PROPERTY(double Balance READ balance)
    inline double balance() const
    { return qvariant_cast< double >(property("Balance")); }

    Q_PROPERTY(double BaseVolume READ baseVolume)
    inline double baseVolume() const
    { return qvariant_cast< double >(property("BaseVolume")); }

    Q_PROPERTY(uint Card READ card)
    inline uint card() const
    { return qvariant_cast< uint >(property("Card")); }

    Q_PROPERTY(QString Description READ description)
    inline QString description() const
    { return qvariant_cast< QString >(property("Description")); }

    Q_PROPERTY(double Fade READ fade)
    inline double fade() const
    { return qvariant_cast< double >(property("Fade")); }

    Q_PROPERTY(bool Mute READ mute)
    inline bool mute() const
    { return qvariant_cast< bool >(property("Mute")); }

    Q_PROPERTY(QString Name READ name)
    inline QString name() const
    { return qvariant_cast< QString >(property("Name")); }

    Q_PROPERTY(AudioPortList Ports READ ports)
    inline AudioPortList ports() const
    { return qvariant_cast< AudioPortList >(property("Ports")); }

    Q_PROPERTY(bool SupportBalance READ supportBalance)
    inline bool supportBalance() const
    { return qvariant_cast< bool >(property("SupportBalance")); }

    Q_PROPERTY(bool SupportFade READ supportFade)
    inline bool supportFade() const
    { return qvariant_cast< bool >(property("SupportFade")); }

    Q_PROPERTY(double Volume READ volume)
    inline double volume() const
    { return qvariant_cast< double >(property("Volume")); }

public Q_SLOTS: // METHODS
    inline QDBusPendingReply<QDBusObjectPath> GetMeter()
    {
        QList<QVariant> argumentList;
        return asyncCallWithArgumentList(QStringLiteral("GetMeter"), argumentList);
    }

    inline QDBusPendingReply<> SetBalance(double in0, bool in1)
    {
        QList<QVariant> argumentList;
        argumentList << QVariant::fromValue(in0) << QVariant::fromValue(in1);
        return asyncCallWithArgumentList(QStringLiteral("SetBalance"), argumentList);
    }

    inline QDBusPendingReply<> SetFade(double in0)
    {
        QList<QVariant> argumentList;
        argumentList << QVariant::fromValue(in0);
        return asyncCallWithArgumentList(QStringLiteral("SetFade"), argumentList);
    }

    inline QDBusPendingReply<> SetMute(bool in0)
    {
        QList<QVariant> argumentList;
        argumentList << QVariant::fromValue(in0);
        return asyncCallWithArgumentList(QStringLiteral("SetMute"), argumentList);
    }

    inline QDBusPendingReply<> SetPort(const QString &in0)
    {
        QList<QVariant> argumentList;
        argumentList << QVariant::fromValue(in0);
        return asyncCallWithArgumentList(QStringLiteral("SetPort"), argumentList);
    }

    inline QDBusPendingReply<> SetVolume(double in0, bool in1)
    {
        QList<QVariant> argumentList;
        argumentList << QVariant::fromValue(in0) << QVariant::fromValue(in1);
        return asyncCallWithArgumentList(QStringLiteral("SetVolume"), argumentList);
    }

Q_SIGNALS: // SIGNALS
};

namespace com {
  namespace deepin {
    namespace daemon {
      namespace AudioSource {
        typedef ::ComDeepinDaemonAudioSourceInterface Source;
      }
    }
  }
}
#endif
