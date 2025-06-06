// Copyright © 2018 Red Hat, Inc
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "waylandintegration.h"
#include "waylandintegration_p.h"
#include "utils.h"
#include "../utils/log.h"
#include <QDBusArgument>
#include <QDBusMetaType>

#include <QEventLoop>
#include <QLoggingCategory>
#include <QThread>
#include <QTimer>
#include <QFile>
#include <QPainter>
#include <QImage>
#include <QtConcurrent>
#include <QtDBus>

// KWayland
#ifdef DWAYLAND_SUPPROT
#include <DWayland/Client/connection_thread.h>
#include <DWayland/Client/event_queue.h>
#include <DWayland/Client/registry.h>
#include <DWayland/Client/output.h>
#include <DWayland/Client/remote_access.h>
#include <DWayland/Client/fakeinput.h>
#include <DWayland/Client/outputdevice_v2.h>
#else
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/event_queue.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/output.h>
#include <KWayland/Client/remote_access.h>
#include <KWayland/Client/outputdevice.h>
#endif  // DWAYLAND_SUPPROT

// system
#include <fcntl.h>
#include <unistd.h>

#include <KLocalizedString>

// system
#include <sys/mman.h>
#include <qdir.h>
#include "recordadmin.h"

#include <string.h>
#include <QMutexLocker>

Q_LOGGING_CATEGORY(XdgDesktopPortalKdeWaylandIntegration, "xdp-kde-wayland-integration")

//取消自动析构机制，此处后续还需优化
// Q_GLOBAL_STATIC(WaylandIntegration::WaylandIntegrationPrivate, globalWaylandIntegration)
static WaylandIntegration::WaylandIntegrationPrivate *globalWaylandIntegration =
    new WaylandIntegration::WaylandIntegrationPrivate();

void WaylandIntegration::init(QStringList list)
{
    qCInfo(dsrApp) << "正在初始化wayland集成";
    globalWaylandIntegration->initWayland(list);
}

void WaylandIntegration::init(QStringList list, GstRecordX *gstRecord)
{
    qCInfo(dsrApp) << "正在初始化wayland集成和GstRecordX";
    globalWaylandIntegration->initWayland(list, gstRecord);
}

bool WaylandIntegration::isEGLInitialized()
{
    qCDebug(dsrApp) << "Checking if EGL is initialized";
    return globalWaylandIntegration->isEGLInitialized();
}

void WaylandIntegration::stopStreaming()
{
    qInfo() << __FUNCTION__ << __LINE__ << "正在停止wayland录屏流程...";
    globalWaylandIntegration->stopStreaming();
    globalWaylandIntegration->stopVideoRecord();
    globalWaylandIntegration->m_appendFrameToListFlag = false;
    qInfo() << __FUNCTION__ << __LINE__ << "已停止wayland录屏流程";
}

bool WaylandIntegration::WaylandIntegrationPrivate::stopVideoRecord()
{
    if (Utils::isFFmpegEnv) {
        if (m_recordAdmin) {
            qCDebug(dsrApp) << "Stopping FFmpeg video recording";
            return m_recordAdmin->stopStream();
        } else {
            qCWarning(dsrApp) << "m_recordAdmin is not init!";
            return false;
        }
    } else {
        qCDebug(dsrApp) << "Stopping GStreamer video recording";
        setBGetFrame(false);
        isGstWriteVideoFrame = false;
        return true;
    }
}

QMap<quint32, WaylandIntegration::WaylandOutput> WaylandIntegration::screens()
{
    return globalWaylandIntegration->screens();
}

WaylandIntegration::WaylandIntegration *WaylandIntegration::waylandIntegration()
{
    return globalWaylandIntegration;
}

// Thank you kscreen
void WaylandIntegration::WaylandOutput::setOutputType(const QString &type)
{
    qCDebug(dsrApp) << "Setting output type:" << type;
    const auto embedded = {QLatin1String("LVDS"), QLatin1String("IDP"), QLatin1String("EDP"), QLatin1String("LCD")};

    for (const QLatin1String &pre : embedded) {
        if (type.toUpper().startsWith(pre)) {
            m_outputType = OutputType::Laptop;
            qCDebug(dsrApp) << "Output type set to Laptop";
            return;
        }
    }

    if (type.contains(QLatin1String("VGA")) || type.contains(QLatin1String("DVI")) || type.contains(QLatin1String("HDMI")) ||
        type.contains(QLatin1String("Panel")) || type.contains(QLatin1String("DisplayPort")) ||
        type.startsWith(QLatin1String("DP")) || type.contains(QLatin1String("unknown"))) {
        m_outputType = OutputType::Monitor;
        qCDebug(dsrApp) << "Output type set to Monitor";
    } else if (type.contains(QLatin1String("TV"))) {
        m_outputType = OutputType::Television;
        qCDebug(dsrApp) << "Output type set to Television";
    } else {
        m_outputType = OutputType::Monitor;
        qCDebug(dsrApp) << "Output type defaulting to Monitor";
    }
}

const QDBusArgument &operator>>(const QDBusArgument &arg, WaylandIntegration::WaylandIntegrationPrivate::Stream &stream)
{
    arg.beginStructure();
    arg >> stream.nodeId;

    arg.beginMap();
    while (!arg.atEnd()) {
        QString key;
        QVariant map;
        arg.beginMapEntry();
        arg >> key >> map;
        arg.endMapEntry();
        stream.map.insert(key, map);
    }
    arg.endMap();
    arg.endStructure();

    return arg;
}

const QDBusArgument &operator<<(QDBusArgument &arg, const WaylandIntegration::WaylandIntegrationPrivate::Stream &stream)
{
    arg.beginStructure();
    arg << stream.nodeId;
    arg << stream.map;
    arg.endStructure();

    return arg;
}

Q_DECLARE_METATYPE(WaylandIntegration::WaylandIntegrationPrivate::Stream)
Q_DECLARE_METATYPE(WaylandIntegration::WaylandIntegrationPrivate::Streams)

WaylandIntegration::WaylandIntegrationPrivate::WaylandIntegrationPrivate()
    : WaylandIntegration()
    , m_eglInitialized(false)
    , m_registryInitialized(false)
    , m_connection(nullptr)
    , m_queue(nullptr)
    , m_registry(nullptr)
    , m_remoteAccessManager(nullptr)
{
    m_bInit = true;
#if defined(__mips__) || defined(__sw_64__) || defined(__loongarch_64__) || defined(__loongarch__)
    m_bufferSize = 60;
#elif defined(__aarch64__)
    m_bufferSize = 60;
#else
    m_bufferSize = 200;
#endif
    m_ffmFrame = nullptr;
    qDBusRegisterMetaType<WaylandIntegrationPrivate::Stream>();
    qDBusRegisterMetaType<WaylandIntegrationPrivate::Streams>();
    m_recordAdmin = nullptr;
    m_gstRecordX = nullptr;
    // m_writeFrameThread = nullptr;
    m_bInitRecordAdmin = true;
    m_bGetFrame = true;
    // m_recordTIme = -1;
    initScreenFrameBuffer();
    m_currentScreenBufs[0] = nullptr;
    m_currentScreenBufs[1] = nullptr;
    m_screenCount = 1;
}

WaylandIntegration::WaylandIntegrationPrivate::~WaylandIntegrationPrivate()
{
    QMutexLocker locker(&m_mutex);
    for (int i = 0; i < m_freeList.size(); i++) {
        if (m_freeList[i]) {
            delete m_freeList[i];
        }
    }
    m_freeList.clear();
    for (int i = 0; i < m_waylandList.size(); i++) {
        delete m_waylandList[i]._frame;
    }
    m_waylandList.clear();
    if (nullptr != m_ffmFrame) {
        delete[] m_ffmFrame;
        m_ffmFrame = nullptr;
    }
    if (nullptr != m_recordAdmin) {
        delete m_recordAdmin;
        m_recordAdmin = nullptr;
    }
}

bool WaylandIntegration::WaylandIntegrationPrivate::isEGLInitialized() const
{
    return m_eglInitialized;
}

void WaylandIntegration::WaylandIntegrationPrivate::bindOutput(int outputName, int outputVersion)
{
    KWayland::Client::Output *output = new KWayland::Client::Output(this);
    output->setup(m_registry->bindOutput(static_cast<uint32_t>(outputName), static_cast<uint32_t>(outputVersion)));
    m_bindOutputs << output;
}

void WaylandIntegration::WaylandIntegrationPrivate::stopStreaming()
{
    qInfo() << "正在停止远程数据流...";
    qDebug() << "m_streamingEnabled: " << m_streamingEnabled;
    if (m_streamingEnabled) {
        m_streamingEnabled = false;

        // First unbound outputs and destroy remote access manager so we no longer receive buffers
        if (m_remoteAccessManager) {
#ifdef KWAYLAND_REMOTE_FLAGE_ON
            qDebug() << "正在释放m_remoteAccessManager...";
            // qDebug() << "m_remoteAccessManager->startRecording(0):" << m_remoteAccessManager->startRecording(0);
            qDebug() << "m_remoteAccessManager->startRecording(1):" << m_remoteAccessManager->startRecording(1);
            // m_remoteAccessManager->startRecording(0);
            qDebug() << "正在等待最后一帧释放...";
            QEventLoop loop;
            connect(this, SIGNAL(lastFrame()), &loop, SLOT(quit()));
            loop.exec();
            m_isAppendRemoteBuffer = false;
            qDebug() << "最后一帧已释放";
            m_remoteAccessManager->release();
            m_remoteAccessManager->destroy();
            qDebug() << "m_remoteAccessManager已释放";
#endif
        }
        qDeleteAll(m_bindOutputs);
        m_bindOutputs.clear();
        //        if (m_stream) {
        //            delete m_stream;
        //            m_stream = nullptr;
        //        }
    }
    qInfo() << "远程数据流已停止";
}

QMap<quint32, WaylandIntegration::WaylandOutput> WaylandIntegration::WaylandIntegrationPrivate::screens()
{
    return m_outputMap;
}

void WaylandIntegration::WaylandIntegrationPrivate::initWayland(QStringList list)
{
    qCInfo(dsrApp) << "Initializing Wayland with parameters";
    m_streamingEnabled = true;
    //通过wayland底层接口获取图片的方式不相同，需要获取电脑的厂商，hw的需要特殊处理
    // m_boardVendorType = getBoardVendorType();
    m_boardVendorType = getProductType();
    qCDebug(dsrApp) << "m_boardVendorType: " << m_boardVendorType;
    //由于性能问题部分非hw的arm机器编码效率低，适当调大视频帧的缓存空间（防止调整的过大导致保存时间延长），且在下面添加视频到缓冲区时进行了降低帧率的处理。
    if (QSysInfo::currentCpuArchitecture().startsWith("ARM", Qt::CaseInsensitive) && !m_boardVendorType) {
        m_bufferSize = 200;
        qCDebug(dsrApp) << "Adjusted buffer size for ARM architecture:" << m_bufferSize;
    }
    m_fps = list[5].toInt();
    qCDebug(dsrApp) << "Setting FPS to:" << m_fps;
    m_recordAdmin = new RecordAdmin(list, this);
    m_recordAdmin->setBoardVendor(m_boardVendorType);
    //初始化wayland服务链接
    initConnectWayland();
    qCDebug(dsrApp) << "Wayland initialization completed";
}

void WaylandIntegration::WaylandIntegrationPrivate::initWayland(QStringList list, GstRecordX *gstRecord)
{
    //通过wayland底层接口获取图片的方式不相同，需要获取电脑的厂商，hw的需要特殊处理
    // m_boardVendorType = getBoardVendorType();
    m_boardVendorType = getProductType();
    qCDebug(dsrApp) << "m_boardVendorType: " << m_boardVendorType;
    //由于性能问题部分非hw的arm机器编码效率低，适当调大视频帧的缓存空间（防止调整的过大导致保存时间延长），且在下面添加视频到缓冲区时进行了降低帧率的处理。
    if (QSysInfo::currentCpuArchitecture().startsWith("ARM", Qt::CaseInsensitive) && !m_boardVendorType) {
        m_bufferSize = 200;
        qCDebug(dsrApp) << "Adjusted buffer size for ARM architecture:" << m_bufferSize;
    }
    m_fps = list[5].toInt();
    m_gstRecordX = gstRecord;
    m_gstRecordX->setBoardVendorType(m_boardVendorType);
    //初始化wayland服务链接
    initConnectWayland();
    qCDebug(dsrApp) << "Wayland initialization with GstRecordX completed";
}

void WaylandIntegration::WaylandIntegrationPrivate::initConnectWayland()
{
    //设置获取视频帧
    setBGetFrame(true);

    m_thread = new QThread(this);
    m_connection = new KWayland::Client::ConnectionThread;
    connect(m_connection,
            &KWayland::Client::ConnectionThread::connected,
            this,
            &WaylandIntegrationPrivate::setupRegistry,
            Qt::QueuedConnection);
    connect(m_connection, &KWayland::Client::ConnectionThread::connectionDied, this, [this] {
        qCWarning(dsrApp) << "Wayland connection died";
        if (m_queue) {
            delete m_queue;
            m_queue = nullptr;
        }
        m_connection->deleteLater();
        m_connection = nullptr;
        if (m_thread) {
            m_thread->quit();
            if (!m_thread->wait(3000)) {
                qCWarning(dsrApp) << "Thread wait timeout, forcing termination";
                m_thread->terminate();
                m_thread->wait();
            }
            delete m_thread;
            m_thread = nullptr;
        }
    });
    connect(m_connection, &KWayland::Client::ConnectionThread::failed, this, [this] {
        qCCritical(dsrApp) << "Wayland connection failed";
        m_thread->quit();
        m_thread->wait();
    });
    m_thread->start();
    m_connection->moveToThread(m_thread);
    m_connection->initConnection();
}

int WaylandIntegration::WaylandIntegrationPrivate::getBoardVendorType()
{
    qCDebug(dsrApp) << "Getting board vendor type";
    QFile file("/sys/class/dmi/id/board_vendor");
    bool flag = file.open(QIODevice::ReadOnly | QIODevice::Text);
    if (!flag) {
        qCWarning(dsrApp) << "Failed to open board vendor file";
        return 0;
    }
    QByteArray t = file.readAll();
    QString result = QString(t);
    if (result.isEmpty()) {
        qCWarning(dsrApp) << "Board vendor file is empty";
        return 0;
    }
    file.close();
    qCDebug(dsrApp) << "/sys/class/dmi/id/board_vendor: " << result;
    qCDebug(dsrApp) << "cpu: " << Utils::getCpuModelName();
    if (result.contains("HUAWEI")) {
        if (!Utils::getCpuModelName().contains("Kunpeng", Qt::CaseSensitivity::CaseInsensitive)) {
            qCDebug(dsrApp) << "Detected HUAWEI non-Kunpeng board";
            return 1;
        }
    }
    qCDebug(dsrApp) << "Using default board vendor type";
    return 0;
}

//根据命令行 dmidecode -s system-product-name|awk '{print SNF}' 返回的结果判断是否是华为电脑
int WaylandIntegration::WaylandIntegrationPrivate::getProductType()
{
    qCDebug(dsrApp) << "Getting product type";
    if (Utils::specialRecordingScreenMode != -1) {
        qCInfo(dsrApp) << "Special screen recording mode is supported!";
        return Utils::specialRecordingScreenMode;
    }
    qCInfo(dsrApp) << "Special screen recording mode is not supported!";
    QProcess process;
    process.start("dmidecode", QStringList() << "-s" << "system-product-name");
    process.waitForFinished();
    process.waitForReadyRead();
    QByteArray tempArray = process.readAllStandardOutput();
    QString str_output = QString(QLatin1String(tempArray.data()));
    qCInfo(dsrApp) << "system-product-name: " << str_output;

    process.start("dmidecode");
    process.waitForFinished();
    process.waitForReadyRead();
    QString str_output1 = QString(QLatin1String(process.readAllStandardOutput().data()));
    qCDebug(dsrApp) << "Full dmidecode output:" << str_output1;

    if (str_output.contains("KLVV", Qt::CaseInsensitive) || str_output.contains("KLVU", Qt::CaseInsensitive) ||
        str_output.contains("PGUV", Qt::CaseInsensitive) || str_output.contains("PGUW", Qt::CaseInsensitive) ||
        str_output1.contains("PWC30", Qt::CaseInsensitive) || str_output.contains("L540", Qt::CaseInsensitive) ||
        str_output.contains("W585", Qt::CaseInsensitive) || str_output.contains("PGUX", Qt::CaseInsensitive) || isPangu()) {
        qCDebug(dsrApp) << "Detected supported product type";
        return 1;
    }
    qCDebug(dsrApp) << "Using default product type";
    return 0;
}

bool WaylandIntegration::WaylandIntegrationPrivate::isPangu()
{
    qCDebug(dsrApp) << "Checking if system is Pangu";
    QString validFrequency = "CurrentSpeed";
    QDBusInterface systemInfoInterface("com.deepin.daemon.SystemInfo",
                                       "/com/deepin/daemon/SystemInfo",
                                       "org.freedesktop.DBus.Properties",
                                       QDBusConnection::sessionBus());
    qCDebug(dsrApp) << "SystemInfo DBus interface valid:" << systemInfoInterface.isValid();
    QDBusMessage replyCpu = systemInfoInterface.call("Get", "com.deepin.daemon.SystemInfo", "CPUHardware");
    QList<QVariant> outArgsCPU = replyCpu.arguments();
    if (outArgsCPU.count()) {
        QString CPUHardware = outArgsCPU.at(0).value<QDBusVariant>().variant().toString();
        qInfo() << __FUNCTION__ << __LINE__ << "Current CPUHardware: " << CPUHardware;

        if (CPUHardware.contains("PANGU")) {
            qCDebug(dsrApp) << "Pangu CPU detected";
            return true;
        }
    }
    qCDebug(dsrApp) << "Not a Pangu system";
    return false;
}

void WaylandIntegration::WaylandIntegrationPrivate::addOutput(quint32 name, quint32 version)
{
    qCDebug(dsrApp) << "Adding output with name:" << name << "version:" << version;
    KWayland::Client::Output *output = new KWayland::Client::Output(this);
    output->setup(m_registry->bindOutput(name, version));

    connect(output, &KWayland::Client::Output::changed, this, [this, name, version, output]() {
        qCDebug(XdgDesktopPortalKdeWaylandIntegration) << "Adding output:";
        qCDebug(XdgDesktopPortalKdeWaylandIntegration) << "    manufacturer: " << output->manufacturer();
        qCDebug(XdgDesktopPortalKdeWaylandIntegration) << "    model: " << output->model();
        qCDebug(XdgDesktopPortalKdeWaylandIntegration) << "    resolution: " << output->pixelSize();

        WaylandOutput portalOutput;
        portalOutput.setManufacturer(output->manufacturer());
        portalOutput.setModel(output->model());
        portalOutput.setOutputType(output->model());
        portalOutput.setResolution(output->pixelSize());
        portalOutput.setWaylandOutputName(static_cast<int>(name));
        portalOutput.setWaylandOutputVersion(static_cast<int>(version));

        if (m_registry->hasInterface(KWayland::Client::Registry::Interface::RemoteAccessManager)) {
            KWayland::Client::Registry::AnnouncedInterface interface =
                m_registry->interface(KWayland::Client::Registry::Interface::RemoteAccessManager);
            if (!interface.name && !interface.version) {
                qCWarning(XdgDesktopPortalKdeWaylandIntegration)
                    << "Failed to start streaming: remote access manager interface is not initialized yet";
                return;
            }
            return;
        }
        m_outputMap.insert(name, portalOutput);

        //        delete output;
    });
}

void WaylandIntegration::WaylandIntegrationPrivate::removeOutput(quint32 name)
{
    WaylandOutput output = m_outputMap.take(name);
    qCDebug(XdgDesktopPortalKdeWaylandIntegration) << "Removing output:";
    qCDebug(XdgDesktopPortalKdeWaylandIntegration) << "    manufacturer: " << output.manufacturer();
    qCDebug(XdgDesktopPortalKdeWaylandIntegration) << "    model: " << output.model();
}

void WaylandIntegration::WaylandIntegrationPrivate::onDeviceChanged(quint32 name, quint32 version)
{
#ifdef DWAYLAND_SUPPORT
    KWayland::Client::OutputDeviceV2 *devT = m_registry->createOutputDeviceV2(name, version);
#else
    KWayland::Client::OutputDevice *devT = m_registry->createOutputDevice(name, version);
#endif // DWAYLAND_SUPPORT

    if (devT && devT->isValid()) {
#ifdef DWAYLAND_SUPPORT
        connect(devT, &KWayland::Client::OutputDeviceV2::changed, this, [=]() {
#else
        connect(devT, &KWayland::Client::OutputDevice::changed, this, [=]() {
#endif // DWAYLAND_SUPPORT
            qDebug() << devT->uuid() << devT->geometry();
            // 保存屏幕id和对应位置大小
            m_screenId2Point.insert(devT->uuid(), devT->geometry());
            m_screenCount = m_screenId2Point.size();

            // 更新屏幕大小
            m_screenSize.setWidth(0);
            m_screenSize.setHeight(0);
            for (auto p = m_screenId2Point.begin(); p != m_screenId2Point.end(); ++p) {
                if (p.value().x() + p.value().width() > m_screenSize.width()) {
                    m_screenSize.setWidth(p.value().x() + p.value().width());
                }
                if (p.value().y() + p.value().height() > m_screenSize.height()) {
                    m_screenSize.setHeight(p.value().y() + p.value().height());
                }
            }
            qDebug() << "屏幕大小:" << m_screenSize;
        });
    }
}

void WaylandIntegration::WaylandIntegrationPrivate::processBuffer(const KWayland::Client::RemoteBuffer *rbuf, const QRect rect)
{
    qInfo() << __FUNCTION__ << __LINE__ << "开始处理buffer...";
    qDebug() << ">>>>>> open fd!" << rbuf->fd();
    //    QScopedPointer<const KWayland::Client::RemoteBuffer> guard(rbuf);
    auto dma_fd = rbuf->fd();
    quint32 width = rbuf->width();
    quint32 height = rbuf->height();
    quint32 stride = rbuf->stride();
    //    if(!bGetFrame())
    //        return;
    if (m_bInitRecordAdmin) {
        m_bInitRecordAdmin = false;
        if (Utils::isFFmpegEnv) {
            qCDebug(dsrApp) << "Using FFmpeg environment";
            m_recordAdmin->init(static_cast<int>(m_screenSize.width()), static_cast<int>(m_screenSize.height()));
            frameStartTime = avlibInterface::m_av_gettime();
        } else {
            qCDebug(dsrApp) << "Using GStreamer environment";
            frameStartTime = QDateTime::currentMSecsSinceEpoch();
            isGstWriteVideoFrame = true;
            if (m_gstRecordX) {
                m_gstRecordX->waylandGstStartRecord();
            } else {
                qCWarning(dsrApp) << "m_gstRecordX is nullptr!";
            }
            QtConcurrent::run(this, &WaylandIntegrationPrivate::gstWriteVideoFrame);
        }
        m_appendFrameToListFlag = true;
        QtConcurrent::run(this, &WaylandIntegrationPrivate::appendFrameToList);
    }
    unsigned char *mapData = static_cast<unsigned char *>(mmap(nullptr, stride * height, PROT_READ, MAP_SHARED, dma_fd, 0));
    if (MAP_FAILED == mapData) {
        qCWarning(XdgDesktopPortalKdeWaylandIntegration) << "dma fd " << dma_fd << " mmap failed - ";
    }
    // QString pngName = "/home/uos/Desktop/test/"+QDateTime::currentDateTime().toString(QLatin1String("hh:mm:ss.zzz ") +
    // QString("%1_").arg(rect.x())); QImage(mapData, width, height, QImage::Format_RGB32).copy().save(pngName + ".png");
    if (m_screenCount == 1 || !m_isScreenExtension) {
        //单屏
        m_curNewImage.first = 0;  // avlibInterface::m_av_gettime() - frameStartTime;
        {
            QMutexLocker locker(&m_bGetScreenImageMutex);
            m_curNewImage.second = QImage(mapData, width, height, QImage::Format_RGBA8888).copy();
        }
    } else {
        // QString pngName = QDateTime::currentDateTime().toString(QLatin1String("hh:mm:ss.zzz ") + QString("%1_").arg(rect.x()));
        // QImage(mapData, width, height, QImage::Format_RGBA8888).copy().save(pngName + ".png");
        m_ScreenDateBuf.append(QPair<QRect, QImage>(rect, QImage(mapData, width, height, getImageFormat(rbuf->format())).copy()));
        if (m_ScreenDateBuf.size() == m_screenCount) {
            QMutexLocker locker(&m_bGetScreenImageMutex);
            m_curNewImageScreen.clear();
            for (auto itr = m_ScreenDateBuf.begin(); itr != m_ScreenDateBuf.end(); ++itr) {
                m_curNewImageScreen.append(*itr);
            }
            m_ScreenDateBuf.clear();
        }
    }
    munmap(mapData, stride * height);
}

void WaylandIntegration::WaylandIntegrationPrivate::processBufferHw(const KWayland::Client::RemoteBuffer *rbuf,
                                                                    const QRect rect,
                                                                    int screenId)
{
    qInfo() << __FUNCTION__ << __LINE__ << "开始处理buffer...";
    qDebug() << ">>>>>> open fd!" << rbuf->fd();
    //    QScopedPointer<const KWayland::Client::RemoteBuffer> guard(rbuf);
    auto dma_fd = rbuf->fd();
    quint32 width = rbuf->width();
    quint32 height = rbuf->height();
    quint32 stride = rbuf->stride();

    if (m_bInitRecordAdmin) {
        qCDebug(dsrApp) << "Initializing record admin";
        m_bInitRecordAdmin = false;
        if (Utils::isFFmpegEnv) {
            qCDebug(dsrApp) << "Using FFmpeg environment";
            m_recordAdmin->init(static_cast<int>(m_screenSize.width()), static_cast<int>(m_screenSize.height()));
            frameStartTime = avlibInterface::m_av_gettime();
        } else {
            qCDebug(dsrApp) << "Using GStreamer environment";
            frameStartTime = QDateTime::currentMSecsSinceEpoch();
            isGstWriteVideoFrame = true;
            if (m_gstRecordX) {
                m_gstRecordX->waylandGstStartRecord();
            } else {
                qWarning() << "m_gstRecordX is nullptr!";
            }
            QtConcurrent::run(this, &WaylandIntegrationPrivate::gstWriteVideoFrame);
        }
        m_appendFrameToListFlag = true;
        QtConcurrent::run(this, &WaylandIntegrationPrivate::appendFrameToList);
    }
    unsigned char *mapData = static_cast<unsigned char *>(mmap(nullptr, stride * height, PROT_READ, MAP_SHARED, dma_fd, 0));
    if (MAP_FAILED == mapData) {
        qCWarning(XdgDesktopPortalKdeWaylandIntegration) << "dma fd " << dma_fd << " mmap failed - ";
    }
    // QString pngName = "/home/uos/Desktop/test/"+QDateTime::currentDateTime().toString(QLatin1String("hh:mm:ss.zzz ") +
    // QString("%1_").arg(rect.x())); QImage(mapData, width, height, QImage::Format_RGB32).copy().save(pngName + ".png");
    if (m_screenCount == 1 || !m_isScreenExtension) {
        qCDebug(dsrApp) << "Processing single screen hardware buffer";
        if (!m_isExistFirstScreenData) {
            qCDebug(dsrApp) << "Allocating first screen data buffer";
            m_firstScreenData = new unsigned char[stride * height];
            m_isExistFirstScreenData = true;
#ifdef KWAYLAND_REMOTE_BUFFER_RELEASE_FLAGE_ON
            m_lastScreenDatas[0]._frame = new unsigned char[stride * height];
            m_lastScreenDatas[0]._width = width;
            m_lastScreenDatas[0]._height = height;
            m_lastScreenDatas[0]._stride = height;
            m_lastScreenDatas[0]._format = getImageFormat(rbuf->format());
            m_lastScreenDatas[0]._rect = rect;
#endif
        }
        //单屏
        // qDebug() << ">>>>>> new unsigned char: " << stride *height;
        // memcpy(m_firstScreenData, mapData, stride * height);
        copyBuffer(m_firstScreenData, mapData, rbuf);
#ifdef KWAYLAND_REMOTE_BUFFER_RELEASE_FLAGE_ON
        // memcpy(m_lastScreenDatas[0]._frame, mapData, stride * height);
        copyBuffer(m_lastScreenDatas[0]._frame, mapData, rbuf);
#endif
        // qDebug() << ">>>>>> memcpy: " << stride *height;
        QMutexLocker locker(&m_bGetScreenImageMutex);
        m_curNewImageData._frame = m_firstScreenData;
        m_curNewImageData._width = width;
        m_curNewImageData._height = height;
        m_curNewImageData._format = QImage::Format_RGBA8888;
        m_curNewImageData._rect = QRect(0, 0, 0, 0);
    } else if (m_screenCount == 2 && m_isScreenExtension) {
        //避免重复开辟内存空间,采用固定数组减小数据访问复杂度。两个屏幕的数据大小可能不同，所以需要区分开
        if (screenId == 0) {
            if (!m_isExistFirstScreenData) {
                qCDebug(dsrApp) << "Allocating first screen data buffer";
                m_firstScreenData = new unsigned char[stride * height];
                m_isExistFirstScreenData = true;
#ifdef KWAYLAND_REMOTE_BUFFER_RELEASE_FLAGE_ON
                m_lastScreenDatas[0]._frame = new unsigned char[stride * height];
                m_lastScreenDatas[0]._width = width;
                m_lastScreenDatas[0]._height = height;
                m_lastScreenDatas[0]._stride = height;
                m_lastScreenDatas[0]._format = getImageFormat(rbuf->format());
                m_lastScreenDatas[0]._rect = rect;
#endif
            }
            // qDebug() << ">>>>>> memcpy1: " << stride *height;
            // memcpy(m_firstScreenData, mapData, stride * height);
            copyBuffer(m_firstScreenData, mapData, rbuf);
#ifdef KWAYLAND_REMOTE_BUFFER_RELEASE_FLAGE_ON
            // memcpy(m_lastScreenDatas[0]._frame, mapData, stride * height);
            copyBuffer(m_lastScreenDatas[0]._frame, mapData, rbuf);
#endif
            // qDebug() << ">>>>>> memcpy2: " << stride *height;
            m_ScreenDateBufFrames[0]._frame = m_firstScreenData;
            m_ScreenDateBufFrames[0]._width = width;
            m_ScreenDateBufFrames[0]._height = height;
            m_ScreenDateBufFrames[0]._format = getImageFormat(rbuf->format());
            m_ScreenDateBufFrames[0]._rect = rect;
            m_ScreenDateBufFrames[0]._flag = true;
        } else {
            if (!m_isExistSecondScreenData) {
                qCDebug(dsrApp) << "Allocating second screen data buffer";
                m_secondScreenData = new unsigned char[stride * height];
                m_isExistSecondScreenData = true;
#ifdef KWAYLAND_REMOTE_BUFFER_RELEASE_FLAGE_ON
                m_lastScreenDatas[1]._frame = new unsigned char[stride * height];
                m_lastScreenDatas[1]._width = width;
                m_lastScreenDatas[1]._height = height;
                m_lastScreenDatas[1]._stride = height;
                m_lastScreenDatas[1]._format = getImageFormat(rbuf->format());
                m_lastScreenDatas[1]._rect = rect;
#endif
            }
            // qDebug() << ">>>>>> memcpy1: " << stride *height;
            // memcpy(m_secondScreenData, mapData, stride * height);
            copyBuffer(m_secondScreenData, mapData, rbuf);
#ifdef KWAYLAND_REMOTE_BUFFER_RELEASE_FLAGE_ON
            // memcpy(m_lastScreenDatas[1]._frame, mapData, stride * height);
            copyBuffer(m_lastScreenDatas[1]._frame, mapData, rbuf);
#endif
            // qDebug() << ">>>>>> memcpy2: " << stride *height;
            m_ScreenDateBufFrames[1]._frame = m_secondScreenData;
            m_ScreenDateBufFrames[1]._width = width;
            m_ScreenDateBufFrames[1]._height = height;
            m_ScreenDateBufFrames[1]._format = getImageFormat(rbuf->format());
            m_ScreenDateBufFrames[1]._rect = rect;
            m_ScreenDateBufFrames[1]._flag = true;
        }

        if (m_ScreenDateBufFrames[0]._flag && m_ScreenDateBufFrames[1]._flag) {
            qCDebug(dsrApp) << "Both screen buffers ready, merging frames";
            QMutexLocker locker(&m_bGetScreenImageMutex);
            for (int i = 0; i < 2; i++) {
                m_curNewImageScreenFrames[i]._frame = m_ScreenDateBufFrames[i]._frame;
                m_curNewImageScreenFrames[i]._width = m_ScreenDateBufFrames[i]._width;
                m_curNewImageScreenFrames[i]._height = m_ScreenDateBufFrames[i]._height;
                m_curNewImageScreenFrames[i]._format = m_ScreenDateBufFrames[i]._format;
                m_curNewImageScreenFrames[i]._rect = m_ScreenDateBufFrames[i]._rect;
                m_curNewImageScreenFrames[i]._flag = m_ScreenDateBufFrames[i]._flag;
                m_ScreenDateBufFrames[i]._flag = false;
            }
        }
    } else {
        qWarning() << "screen count > 2!!!";
    }
#ifdef KWAYLAND_REMOTE_BUFFER_RELEASE_FLAGE_ON
    if (screenId == 0) {
        if (m_currentScreenBufs[0] != nullptr) {
            close(m_currentScreenBufs[0]->fd());
            m_currentScreenBufs[0]->release();
            m_currentScreenBufs[0] = nullptr;
            // qDebug() << "m_currentScreenBufs[0] is release!";
            m_mutex.unlock();
        }
    } else {
        if (m_currentScreenBufs[1] != nullptr) {
            close(m_currentScreenBufs[1]->fd());
            m_currentScreenBufs[1]->release();
            m_currentScreenBufs[1] = nullptr;
            // qDebug() << "m_currentScreenBufs[1] is release!";
            m_mutex.unlock();
        }
    }
#endif
    munmap(mapData, stride * height);
    qInfo() << __FUNCTION__ << __LINE__ << "已处理buffer";
}

//拷贝数据，当远程buffer没有数据传过来时，调用此接口，将上一次的屏幕数据做为当前的屏幕数据
void WaylandIntegration::WaylandIntegrationPrivate::copyScreenData(int screenId)
{
    qCDebug(dsrApp) << "Copying screen data for screen" << screenId;
    if (m_screenCount == 1 || !m_isScreenExtension) {
        qCDebug(dsrApp) << "Copying single screen data";
        m_curNewImageData._frame = m_lastScreenDatas[0]._frame;
        m_curNewImageData._width = m_lastScreenDatas[0]._width;
        m_curNewImageData._height = m_lastScreenDatas[0]._height;
        m_curNewImageData._format = m_lastScreenDatas[0]._format;
        m_curNewImageData._rect = m_lastScreenDatas[0]._rect;
        m_curNewImageData._flag = true;
    } else if (m_screenCount == 2 && m_isScreenExtension) {
        qCDebug(dsrApp) << "Copying multi-screen data for screen" << screenId;
        if (screenId == 0) {
            m_ScreenDateBufFrames[0]._frame = m_lastScreenDatas[0]._frame;
            m_ScreenDateBufFrames[0]._width = m_lastScreenDatas[0]._width;
            m_ScreenDateBufFrames[0]._height = m_lastScreenDatas[0]._height;
            m_ScreenDateBufFrames[0]._format = m_lastScreenDatas[0]._format;
            m_ScreenDateBufFrames[0]._rect = m_lastScreenDatas[0]._rect;
            m_ScreenDateBufFrames[0]._flag = true;
        } else {
            m_ScreenDateBufFrames[1]._frame = m_lastScreenDatas[1]._frame;
            m_ScreenDateBufFrames[1]._width = m_lastScreenDatas[1]._width;
            m_ScreenDateBufFrames[1]._height = m_lastScreenDatas[1]._height;
            m_ScreenDateBufFrames[1]._format = m_lastScreenDatas[1]._format;
            m_ScreenDateBufFrames[1]._rect = m_lastScreenDatas[1]._rect;
            m_ScreenDateBufFrames[1]._flag = true;
        }
        if (m_ScreenDateBufFrames[0]._flag && m_ScreenDateBufFrames[1]._flag && m_lastScreenDatas[0]._frame != nullptr &&
            m_lastScreenDatas[1]._frame != nullptr) {
            qCDebug(dsrApp) << "Both screen buffers ready, merging frames";
            QMutexLocker locker(&m_bGetScreenImageMutex);
            for (int i = 0; i < 2; i++) {
                m_curNewImageScreenFrames[i]._frame = m_ScreenDateBufFrames[i]._frame;
                m_curNewImageScreenFrames[i]._width = m_ScreenDateBufFrames[i]._width;
                m_curNewImageScreenFrames[i]._height = m_ScreenDateBufFrames[i]._height;
                m_curNewImageScreenFrames[i]._format = m_ScreenDateBufFrames[i]._format;
                m_curNewImageScreenFrames[i]._rect = m_ScreenDateBufFrames[i]._rect;
                m_curNewImageScreenFrames[i]._flag = m_ScreenDateBufFrames[i]._flag;
                m_ScreenDateBufFrames[i]._flag = false;
            }
        }
    }
    qCDebug(dsrApp) << "Screen data copy completed";
}

//根据wayland客户端bufferReady给过来的像素格式，转成QImage的格式
QImage::Format WaylandIntegration::WaylandIntegrationPrivate::getImageFormat(quint32 format)
{
    switch (format) {
        case GBM_FORMAT_XRGB8888:  // GBM_FORMAT_XRGB8888 = 875713112
            qDebug() << "fd 图片格式: XRGB" << GBM_FORMAT_XRGB8888 << " -> "
                     << "QImage::Format_RGB32";
            return QImage::Format_RGB32;
        case GBM_FORMAT_XBGR8888:  // GBM_FORMAT_XBGR8888 = 875709016
            qDebug() << "fd 图片格式: XBGR" << GBM_FORMAT_XBGR8888 << " -> "
                     << "QImage::Format_RGB32";
            return QImage::Format_RGB32;
        case GBM_FORMAT_RGBX8888:  // GBM_FORMAT_RGBX8888 = 875714642
            qDebug() << "fd 图片格式: RGBX" << GBM_FORMAT_RGBX8888 << " -> "
                     << "QImage::Format_RGBX8888";
            return QImage::Format_RGBX8888;
        case GBM_FORMAT_BGRX8888:  // GBM_FORMAT_BGRX8888 = 875714626
            qDebug() << "fd 图片格式: BGRX" << GBM_FORMAT_BGRX8888 << " -> "
                     << "QImage::Format_BGR30";
            return QImage::Format_BGR30;
        case GBM_FORMAT_ARGB8888:  // GBM_FORMAT_ARGB8888 = 875713089
            qDebug() << "fd 图片格式: ARGB" << GBM_FORMAT_ARGB8888 << " -> "
                     << "QImage::Format_ARGB32";
            return QImage::Format_ARGB32;
        case GBM_FORMAT_ABGR8888:  // GBM_FORMAT_ABGR8888 = 875708993
            qDebug() << "fd 图片格式: ABGR" << GBM_FORMAT_ABGR8888 << " -> "
                     << "QImage::Format_ARGB32";
            return QImage::Format_ARGB32;
        case GBM_FORMAT_RGBA8888:  // GBM_FORMAT_RGBA8888 = 875708754
            qDebug() << "fd 图片格式: RGBA" << GBM_FORMAT_RGBA8888 << " -> "
                     << "QImage::Format_RGBA8888";
            return QImage::Format_RGBA8888;
        case GBM_FORMAT_BGRA8888:  // GBM_FORMAT_BGRA8888 = 875708738
            qDebug() << "fd 图片格式: BGRA" << GBM_FORMAT_BGRA8888 << " -> "
                     << "QImage::Format_BGR30";
            return QImage::Format_BGR30;
        default:
            qDebug() << "fd 图片格式未知: " << format << " -> "
                     << "QImage::Format_RGB32";
            return QImage::Format_RGB32;
    }
}

void WaylandIntegration::WaylandIntegrationPrivate::processBufferX86(const KWayland::Client::RemoteBuffer *rbuf, const QRect rect)
{
    qInfo() << __FUNCTION__ << __LINE__ << "开始处理buffer...";
    qDebug() << ">>>>>> open fd!" << rbuf->fd();
    // QScopedPointer<const KWayland::Client::RemoteBuffer> guard(rbuf);
    auto dma_fd = rbuf->fd();
    quint32 width = rbuf->width();
    quint32 height = rbuf->height();
    quint32 stride = rbuf->stride();
    if (m_bInitRecordAdmin) {
        qCDebug(dsrApp) << "Initializing record admin";
        m_bInitRecordAdmin = false;
        if (Utils::isFFmpegEnv) {
            qCDebug(dsrApp) << "Using FFmpeg environment";
            m_recordAdmin->init(static_cast<int>(m_screenSize.width()), static_cast<int>(m_screenSize.height()));
            frameStartTime = avlibInterface::m_av_gettime();
        } else {
            qCDebug(dsrApp) << "Using GStreamer environment";
            frameStartTime = QDateTime::currentMSecsSinceEpoch();
            isGstWriteVideoFrame = true;
            if (m_gstRecordX) {
                m_gstRecordX->waylandGstStartRecord();
            } else {
                qWarning() << "m_gstRecordX is nullptr!";
            }
            QtConcurrent::run(this, &WaylandIntegrationPrivate::gstWriteVideoFrame);
        }
        m_appendFrameToListFlag = true;
        QtConcurrent::run(this, &WaylandIntegrationPrivate::appendFrameToList);
    }

    QImage img(m_screenSize, QImage::Format_RGBA8888);
    if (m_screenCount == 1 || !m_isScreenExtension) {
        m_curNewImage.first = 0;  // avlibInterface::m_av_gettime() - frameStartTime;
        {
            QMutexLocker locker(&m_bGetScreenImageMutex);
            m_curNewImage.second = getImage(dma_fd, width, height, stride, rbuf->format());
        }
    } else {
        qCDebug(dsrApp) << "Processing multi-screen x86 buffer";
        m_ScreenDateBuf.append(QPair<QRect, QImage>(rect, getImage(dma_fd, width, height, stride, rbuf->format())));
        if (m_ScreenDateBuf.size() == m_screenCount) {
            qCDebug(dsrApp) << "All screen buffers collected, merging images";
            QMutexLocker locker(&m_bGetScreenImageMutex);
            m_curNewImageScreen.clear();
            for (auto itr = m_ScreenDateBuf.begin(); itr != m_ScreenDateBuf.end(); ++itr) {
                m_curNewImageScreen.append(*itr);
            }
            m_ScreenDateBuf.clear();
        }
    }
}
//通过线程每30ms钟向数据池中取出一张图片添加到环形缓冲区，以便后续视频编码
void WaylandIntegration::WaylandIntegrationPrivate::appendFrameToList()
{
    int64_t delayTime = 1000 / m_fps + 1;
#ifdef __mips__
    // delayTime = 150;
#endif

    while (m_appendFrameToListFlag) {
        if (m_screenCount == 1 || !m_isScreenExtension) {
            QImage tempImage;
            {
                QMutexLocker locker(&m_bGetScreenImageMutex);
                if (m_boardVendorType) {
                    tempImage = QImage(
                        m_curNewImageData._frame, m_curNewImageData._width, m_curNewImageData._height, m_curNewImageData._format);
                } else {
                tempImage = m_curNewImage.second.copy();
                }
            }
            if (!tempImage.isNull()) {
                int64_t temptime = 0;
                if (Utils::isFFmpegEnv) {
                    temptime = avlibInterface::m_av_gettime();

                } else {
                    temptime = QDateTime::currentMSecsSinceEpoch();
                }
                appendBuffer(tempImage.bits(),
                             static_cast<int>(tempImage.width()),
                             static_cast<int>(tempImage.height()),
                             static_cast<int>(tempImage.width() * 4),
                             temptime);
            }
            QThread::msleep(static_cast<unsigned long>(delayTime));
        } else {
            //多屏录制
            int64_t curFramTime = 0;
            if (Utils::isFFmpegEnv) {
                curFramTime = avlibInterface::m_av_gettime();

            } else {
                curFramTime = QDateTime::currentMSecsSinceEpoch();
            }
#if 0
            cv::Mat res;
            res.create(cv::Size(m_screenSize.width(), m_screenSize.height()), CV_8UC4);
            res = cv::Scalar::all(0);
            for (auto itr = m_curNewImageScreen.begin(); itr != m_curNewImageScreen.end(); ++itr) {
                cv::Mat temp = cv::Mat(itr->second.height(), itr->second.width(), CV_8UC4,
                                       const_cast<uchar *>(itr->second.bits()), static_cast<size_t>(itr->second.bytesPerLine()));
                temp.copyTo(res(cv::Rect(itr->first.x(), itr->first.y(), itr->first.width(), itr->first.height())));
            }
            m_curNewImageScreen.clear();
            QImage img = QImage(res.data, res.cols, res.rows, static_cast<int>(res.step), QImage::Format_RGBA8888);
#else

            QVector<QPair<QRect, QImage> > tempImageVec;
            {
                QMutexLocker locker(&m_bGetScreenImageMutex);
                if (m_boardVendorType) {
                    if (!m_curNewImageScreenFrames[0]._flag || !m_curNewImageScreenFrames[1]._flag)
                        continue;
                    for (int i = 0; i < 2; i++) {
                        QImage tempImage = QImage(m_curNewImageScreenFrames[i]._frame,
                                                  static_cast<int>(m_curNewImageScreenFrames[i]._width),
                                                  static_cast<int>(m_curNewImageScreenFrames[i]._height),
                                                  m_curNewImageScreenFrames[i]._format)
                                               .copy();
                        tempImageVec.append(QPair<QRect, QImage>(m_curNewImageScreenFrames[i]._rect, tempImage));
                        m_curNewImageScreenFrames[i]._flag = false;
                    }
                } else {
                    if (m_curNewImageScreen.size() != m_screenCount)
                        continue;
                for (auto itr = m_curNewImageScreen.begin(); itr != m_curNewImageScreen.end(); ++itr) {
                    tempImageVec.append(*itr);
                    }
                }
            }
            QImage img(m_screenSize, QImage::Format_RGBA8888);
            if (m_boardVendorType) {
                img = img.convertToFormat(QImage::Format_RGB32);
            }
            img.fill(Qt::GlobalColor::black);
            QPainter painter(&img);
            for (auto itr = tempImageVec.begin(); itr != tempImageVec.end(); ++itr) {
                painter.drawImage(itr->first.topLeft(), itr->second);
            }
            tempImageVec.clear();
#endif
            appendBuffer(img.bits(), img.width(), img.height(), img.width() * 4, curFramTime /*- frameStartTime*/);
            // 计算拼接的时的耗时
            int64_t t = 0;
            if (Utils::isFFmpegEnv) {
                t = (avlibInterface::m_av_gettime() - curFramTime) / 1000;

            } else {
                t = (QDateTime::currentMSecsSinceEpoch() - curFramTime) / 1000;
            }
            qDebug() << delayTime - t;
            if (t < delayTime) {
                QThread::msleep(static_cast<unsigned long>(delayTime - t));
            }
        }
    }
}

//通过线程循环向gstreamer管道写入视频帧数据
void WaylandIntegration::WaylandIntegrationPrivate::gstWriteVideoFrame()
{
    qCInfo(dsrApp) << "Starting GStreamer video frame write thread";
    waylandFrame frame;
    while (isWriteVideo()) {
        if (getFrame(frame)) {
            qCDebug(dsrApp) << "Writing frame to GStreamer pipeline";
            if (m_gstRecordX) {
                m_gstRecordX->waylandWriteVideoFrame(frame._frame, frame._width, frame._height);
            } else {
                qWarning() << "m_gstRecordX is nullptr!";
            }
        } else {
            // qDebug() << "视频缓冲区无数据！";
        }
    }
    //等待wayland视频环形队列中的视频帧，全部写入管道
    if (m_gstRecordX) {
        qCInfo(dsrApp) << "Stopping GStreamer recording";
        m_gstRecordX->waylandGstStopRecord();
    } else {
        qWarning() << "m_gstRecordX is nullptr!";
    }
    qCInfo(dsrApp) << "GStreamer video frame write thread stopped";
}

QImage WaylandIntegration::WaylandIntegrationPrivate::getImage(
    int32_t fd, uint32_t width, uint32_t height, uint32_t stride, uint32_t format)
{
    QImage tempImage;
    tempImage = QImage(static_cast<int>(width), static_cast<int>(height), QImage::Format_RGBA8888);
    eglMakeCurrent(m_eglstruct.dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, m_eglstruct.ctx);
    EGLint importAttributes[] = {EGL_WIDTH,
                                 static_cast<int>(width),
                                 EGL_HEIGHT,
                                 static_cast<int>(height),
                                 EGL_LINUX_DRM_FOURCC_EXT,
                                 static_cast<int>(format),
                                 EGL_DMA_BUF_PLANE0_FD_EXT,
                                 fd,
                                 EGL_DMA_BUF_PLANE0_OFFSET_EXT,
                                 0,
                                 EGL_DMA_BUF_PLANE0_PITCH_EXT,
                                 static_cast<int>(stride),
                                 EGL_NONE};
    EGLImageKHR image = eglCreateImageKHR(m_eglstruct.dpy, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, importAttributes);
    if (image == EGL_NO_IMAGE_KHR) {
        qDebug() << "未获取到图片！！！";
        return tempImage;
    }

    // create GL 2D texture for framebuffer
    GLuint texture;
    glGenTextures(1, &texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, texture);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);

    GLuint fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, tempImage.bits());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &texture);
    eglDestroyImageKHR(m_eglstruct.dpy, image);

    return tempImage;
}
static int frameCount = 0;
void WaylandIntegration::WaylandIntegrationPrivate::setupRegistry()
{
    qDebug() << "初始化wayland服务链接已完成";
    qDebug() << "正在安装注册wayland服务...";
    if (!m_boardVendorType) {
    initEgl();
    }
    m_queue = new KWayland::Client::EventQueue(this);
    m_queue->setup(m_connection);

    m_registry = new KWayland::Client::Registry(this);

    connect(m_registry, &KWayland::Client::Registry::outputAnnounced, this, &WaylandIntegrationPrivate::addOutput);
    connect(m_registry, &KWayland::Client::Registry::outputRemoved, this, &WaylandIntegrationPrivate::removeOutput);
#ifdef DWAYLAND_SUPPORT
    connect(m_registry, &KWayland::Client::Registry::outputDeviceV2Announced, this, &WaylandIntegrationPrivate::onDeviceChanged);
#else
    connect(m_registry, &KWayland::Client::Registry::outputDeviceAnnounced, this, &WaylandIntegrationPrivate::onDeviceChanged);
#endif // DWAYLAND_SUPPORT
    connect(m_registry, &KWayland::Client::Registry::interfacesAnnounced, this, [this] {
        m_registryInitialized = true;
        // qCDebug(XdgDesktopPortalKdeWaylandIntegration) << "Registry initialized";
    });

    connect(m_registry, &KWayland::Client::Registry::remoteAccessManagerAnnounced, this, [this](quint32 name, quint32 version) {
        Q_UNUSED(name);
        Q_UNUSED(version);
        m_remoteAccessManager = m_registry->createRemoteAccessManager(
            m_registry->interface(KWayland::Client::Registry::Interface::RemoteAccessManager).name,
            m_registry->interface(KWayland::Client::Registry::Interface::RemoteAccessManager).version);
#ifdef KWAYLAND_REMOTE_BUFFER_RELEASE_FLAGE_ON
        connect(m_remoteAccessManager,
                &KWayland::Client::RemoteAccessManager::bufferReady,
                this,
                [this](const void *output, KWayland::Client::RemoteBuffer *rbuf) {
#else
        connect(m_remoteAccessManager, &KWayland::Client::RemoteAccessManager::bufferReady, this, [this](const void *output, const KWayland::Client::RemoteBuffer * rbuf) {
#endif
                    // qDebug() << "正在接收buffer...";
                    QRect screenGeometry =
                        (KWayland::Client::Output::get(reinterpret_cast<wl_output *>(const_cast<void *>(output))))->geometry();
                    // qDebug() << "screenGeometry: " << screenGeometry;
                    // qDebug() << "rbuf->isValid(): " << rbuf->isValid();
            connect(rbuf, &KWayland::Client::RemoteBuffer::parametersObtained, this, [this, rbuf, screenGeometry] {
                        qDebug() << "正在处理buffer..."
                                 << "fd:" << rbuf->fd() << "frameCount: " << frameCount;
                        if (frameCount == 0) {
                            qDebug() << "Current number of screens: " << m_screenCount;
                            for (auto p = m_screenId2Point.begin(); p != m_screenId2Point.end(); ++p) {
                                if (p.value().width() == m_screenSize.width() && p.value().height() == m_screenSize.height()) {
                                    m_isScreenExtension = false;
                                    break;
                                }
                            }
                            qDebug() << "Whether the current screen is in extended mode? " << m_isScreenExtension;
                        }

#ifdef KWAYLAND_REMOTE_BUFFER_RELEASE_FLAGE_ON
                        if (m_screenCount == 1 || !m_isScreenExtension) {
                            if (m_currentScreenBufs[0] == nullptr) {
                                m_isReleaseCurrentBuffer = false;
                                m_currentScreenRects[0] = screenGeometry;
                                m_currentScreenBufs[0] = rbuf;
                                // qDebug() << "m_currentScreenBufs[0] is save! fd:" << rbuf->fd();
                            } else {
                                // qDebug() << "m_currentScreenBufs[0] m_isReleaseCurrentBuffer";
                                m_isReleaseCurrentBuffer = true;
                            }
                            frameCount++;
                            if (m_currentScreenBufs[0] != nullptr && !m_isAppendRemoteBuffer) {
                                m_isAppendRemoteBuffer = true;
                                qInfo() << "How many frames of data are waiting? " << frameCount;
                                frameCount = 0;
                                QtConcurrent::run(this, &WaylandIntegrationPrivate::appendRemoteBuffer);
                            }
                        } else if (m_screenCount == 2 && m_isScreenExtension) {
                            qCDebug(dsrApp) << "Processing multi-screen buffer";
                            if (screenGeometry.x() == 0 && screenGeometry.y() == 0) {
                                if (m_currentScreenBufs[0] == nullptr) {
                                    m_isReleaseCurrentBuffer = false;
                                    m_currentScreenRects[0] = screenGeometry;
                                    m_currentScreenBufs[0] = rbuf;
                                    // qDebug() << "m_currentScreenBufs[0] is save! fd:" << rbuf->fd();
                                } else {
                                    // qDebug() << "m_currentScreenBufs[0] m_isReleaseCurrentBuffer";
                                    m_isReleaseCurrentBuffer = true;
                                }
                            } else {
                                if (m_currentScreenBufs[1] == nullptr) {
                                    m_isReleaseCurrentBuffer = false;
                                    m_currentScreenBufs[1] = rbuf;
                                    m_currentScreenRects[1] = screenGeometry;
                                    // qDebug() << "m_currentScreenBufs[1] is save! fd:" << rbuf->fd();
                                } else {
                                    // qDebug() << "m_currentScreenBufs[1] m_isReleaseCurrentBuffer";
                                    m_isReleaseCurrentBuffer = true;
                                }
                            }
                            frameCount++;
                            if ((m_currentScreenBufs[0] != nullptr && m_currentScreenBufs[1] != nullptr) &&
                                !m_isAppendRemoteBuffer) {
                                m_isAppendRemoteBuffer = true;
                                qInfo() << "How many frames of data are waiting? " << frameCount;
                                frameCount = 0;
                                QtConcurrent::run(this, &WaylandIntegrationPrivate::appendRemoteBuffer);
                            }
                        }
#else
                bool flag = false;
                int screenId = 0;
                if (m_screenCount == 1 || !m_isScreenExtension)
                {
                    //抽帧（数据过来60帧，取一半，由于memcpy一帧数据需要30ms，无法满足1s60帧的速度，所以需要抽帧）
                    flag = (frameCount++ % 2 == 0);
                    if (flag)
                    {
                        if (m_boardVendorType) {
                            //arm hw
                            //processBuffer(rbuf, screenGeometry);
                            processBufferHw(rbuf, screenGeometry, screenId);
                        } else {
                            //other
                    processBufferX86(rbuf, screenGeometry);
                }
                    }
                }
                else if(m_screenCount == 2 && m_isScreenExtension){
                    if (frameCount == 0) {
                        //第一帧画面的坐标不是从（0,0）开始，从第二帧开始取
                        if (screenGeometry.x() != 0 || screenGeometry.y() != 0) {
                            frameCount += 1;
                        }
                    }
                    //抽帧（数据过来60帧，取一半）
                    if (frameCount % 4 == 0) {
                        screenId = 0;
                    } else if ((frameCount - 1) % 4 == 0) {
                        screenId = 1;
                    }
                    flag = (frameCount % 4 == 0) || (((frameCount - 1) != 0) && (frameCount - 1) % 4 == 0);
                    frameCount++;
                    if (flag)
                    {
                        if (m_boardVendorType) {
                            //arm hw
                            //processBuffer(rbuf, screenGeometry);
                            processBufferHw(rbuf, screenGeometry, screenId);
                        } else {
                            //other
                            processBufferX86(rbuf, screenGeometry);
                        }
                    }
                }
#endif
                        else {
                            qWarning() << "Currently,recording with more than two screens is not supported!";
                        }
#ifdef KWAYLAND_REMOTE_FLAGE_ON
                        // qDebug() << "rbuf->frame(): " << rbuf->frame();
                        if (rbuf->frame() == 0) {
                            qDebug() << "是否是最后一帧: " << rbuf->frame();
                            emit lastFrame();
                        }
#endif

                        if (m_isReleaseCurrentBuffer) {
                            // qDebug() << "close(rbuf->fd()): fd=" <<rbuf->fd();
                            close(rbuf->fd());
#ifdef KWAYLAND_REMOTE_BUFFER_RELEASE_FLAGE_ON
                            // qDebug() << "rbuf->release()";
                            rbuf->release();
#endif
                        }
                        qDebug() << "buffer已处理"
                                 << "fd:" << rbuf->fd();
                    });
                    // qDebug() << "buffer已接收";
                });
    });

    m_registry->create(m_connection);
    m_registry->setEventQueue(m_queue);
    m_registry->setup();
}
void WaylandIntegration::WaylandIntegrationPrivate::initEgl()
{
    qCInfo(dsrApp) << "Initializing EGL";
    static const EGLint context_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};

    EGLint config_attribs[] = {EGL_SURFACE_TYPE,
                               EGL_WINDOW_BIT,
                               EGL_RED_SIZE,
                               1,
                               EGL_GREEN_SIZE,
                               1,
                               EGL_BLUE_SIZE,
                               1,
                               EGL_ALPHA_SIZE,
                               1,
                               EGL_RENDERABLE_TYPE,
                               // EGL_OPENGL_ES2_BIT,
                               EGL_OPENGL_BIT,
                               EGL_NONE};

    EGLint major, minor, n;
    EGLBoolean ret;
    m_eglstruct.dpy = eglGetDisplay(reinterpret_cast<EGLNativeDisplayType>(m_connection->display()));
    assert(m_eglstruct.dpy);

    ret = eglInitialize(m_eglstruct.dpy, &major, &minor);
    if (ret != EGL_TRUE) {
        qCCritical(dsrApp) << "Failed to initialize EGL";
        return;
    }
    qCDebug(dsrApp) << "EGL initialized with version" << major << "." << minor;

    // ret = eglBindAPI(EGL_OPENGL_ES_API);
    ret = eglBindAPI(EGL_OPENGL_API);
    if (ret != EGL_TRUE) {
        qCCritical(dsrApp) << "Failed to bind OpenGL API";
        return;
    }

    ret = eglChooseConfig(m_eglstruct.dpy, config_attribs, &m_eglstruct.conf, 1, &n);
    if (!ret || n != 1) {
        qCCritical(dsrApp) << "Failed to choose EGL config";
        return;
    }

    m_eglstruct.ctx = eglCreateContext(m_eglstruct.dpy, m_eglstruct.conf, EGL_NO_CONTEXT, context_attribs);
    if (!m_eglstruct.ctx) {
        qCCritical(dsrApp) << "Failed to create EGL context";
        return;
    }

    printf("egl init success!\n");
}
void WaylandIntegration::WaylandIntegrationPrivate::appendBuffer(
    unsigned char *frame, int width, int height, int stride, int64_t time)
{
    qCDebug(dsrApp) << "Appending buffer with dimensions:" << width << "x" << height;
    if (!bGetFrame() || nullptr == frame || width <= 0 || height <= 0) {
        qCWarning(dsrApp) << "Invalid buffer parameters, skipping append";
        return;
    }
    int size = height * stride;
    unsigned char *ch = nullptr;
    if (m_bInit) {
        qCDebug(dsrApp) << "Initializing buffer with size:" << size;
        m_bInit = false;
        m_width = width;
        m_height = height;
        m_stride = stride;
        m_ffmFrame = new unsigned char[static_cast<unsigned long>(size)];
        for (int i = 0; i < m_bufferSize; i++) {
            ch = new unsigned char[static_cast<unsigned long>(size)];
            m_freeList.append(ch);
            // qDebug() << "创建内存空间";
        }
    }
    QMutexLocker locker(&m_mutex);
    if (m_waylandList.size() >= m_bufferSize) {
        qCDebug(dsrApp) << "Buffer full, recycling first frame";
        waylandFrame wFrame = m_waylandList.first();
        memset(wFrame._frame, 0, static_cast<size_t>(size));
        //拷贝当前帧
        memcpy(wFrame._frame, frame, static_cast<size_t>(size));
        wFrame._time = time;
        wFrame._width = width;
        wFrame._height = height;
        wFrame._stride = stride;
        wFrame._index = 0;
        m_waylandList.first()._frame = nullptr;
        //删队首
        m_waylandList.removeFirst();
        //存队尾
        m_waylandList.append(wFrame);
        // qDebug() << "环形缓冲区已满，删队首，存队尾";
    } else if (0 <= m_waylandList.size() && m_waylandList.size() < m_bufferSize) {
        if (m_freeList.size() > 0) {
            qCDebug(dsrApp) << "Adding new frame to buffer";
            waylandFrame wFrame;
            wFrame._time = time;
            wFrame._width = width;
            wFrame._height = height;
            wFrame._stride = stride;
            wFrame._index = 0;
            //分配空闲内存
            wFrame._frame = m_freeList.first();
            memset(wFrame._frame, 0, static_cast<size_t>(size));
            //拷贝wayland推送的视频帧
            memcpy(wFrame._frame, frame, static_cast<size_t>(size));
            m_waylandList.append(wFrame);
            // qDebug() << "环形缓冲区未满，存队尾"
            //空闲内存占用，仅删除索引，不删除空间
            m_freeList.removeFirst();
        }
    }
    qCDebug(dsrApp) << "Buffer append completed, current size:" << m_waylandList.size();
}

void WaylandIntegration::WaylandIntegrationPrivate::initScreenFrameBuffer()
{
    qCDebug(dsrApp) << "Initializing screen frame buffers";
    m_ScreenDateBufFrames[0]._frame = nullptr;
    m_ScreenDateBufFrames[0]._width = 0;
    m_ScreenDateBufFrames[0]._height = 0;
    m_ScreenDateBufFrames[0]._format = QImage::Format_RGBA8888;
    m_ScreenDateBufFrames[0]._rect = QRect(0, 0, 0, 0);
    m_ScreenDateBufFrames[0]._flag = false;
    m_ScreenDateBufFrames[1]._frame = nullptr;
    m_ScreenDateBufFrames[1]._width = 0;
    m_ScreenDateBufFrames[1]._height = 0;
    m_ScreenDateBufFrames[1]._format = QImage::Format_RGBA8888;
    m_ScreenDateBufFrames[1]._rect = QRect(0, 0, 0, 0);
    m_ScreenDateBufFrames[1]._flag = false;
    m_curNewImageScreenFrames[0]._frame = nullptr;
    m_curNewImageScreenFrames[0]._width = 0;
    m_curNewImageScreenFrames[0]._height = 0;
    m_curNewImageScreenFrames[0]._format = QImage::Format_RGBA8888;
    m_curNewImageScreenFrames[0]._rect = QRect(0, 0, 0, 0);
    m_curNewImageScreenFrames[0]._flag = false;
    m_curNewImageScreenFrames[1]._frame = nullptr;
    m_curNewImageScreenFrames[1]._width = 0;
    m_curNewImageScreenFrames[1]._height = 0;
    m_curNewImageScreenFrames[1]._format = QImage::Format_RGBA8888;
    m_curNewImageScreenFrames[1]._rect = QRect(0, 0, 0, 0);
    m_curNewImageScreenFrames[1]._flag = false;
    qCDebug(dsrApp) << "Screen frame buffers initialized";
}

void WaylandIntegration::WaylandIntegrationPrivate::appendRemoteBuffer()
{
    qInfo() << "Start append remote buffer...";
    while (m_isAppendRemoteBuffer) {
        // qDebug() << "Appending remote buffer...";
        if (m_currentScreenBufs[0] != nullptr) {
            qCDebug(dsrApp) << "Processing first screen buffer";
            m_mutex.lock();
            processBufferHw(m_currentScreenBufs[0], m_currentScreenRects[0], 0);
        } else {
            qCDebug(dsrApp) << "Copying screen data for first screen";
            copyScreenData(0);
        }
        
        if (m_screenCount == 2 && m_isScreenExtension) {
            qCDebug(dsrApp) << "Processing multi-screen setup";
            if (m_currentScreenBufs[1] != nullptr) {
                qCDebug(dsrApp) << "Processing second screen buffer";
                m_mutex.lock();
                processBufferHw(m_currentScreenBufs[1], m_currentScreenRects[1], 1);
            } else {
                qCDebug(dsrApp) << "Copying screen data for second screen";
                copyScreenData(1);
            }
        }
    }
    qInfo() << "Stop append remote buffer";
}

int WaylandIntegration::WaylandIntegrationPrivate::frameIndex = 0;

bool WaylandIntegration::WaylandIntegrationPrivate::getFrame(waylandFrame &frame)
{
    qCDebug(dsrApp) << "Attempting to get frame from buffer";
    QMutexLocker locker(&m_mutex);
    if (m_waylandList.size() <= 0 || nullptr == m_ffmFrame) {
        qCDebug(dsrApp) << "No frames available in buffer or frame buffer not initialized";
        frame._width = 0;
        frame._height = 0;
        frame._frame = nullptr;
        return false;
    } else {
        int size = m_height * m_stride;
        //取队首，先进先出
        waylandFrame wFrame = m_waylandList.first();
        frame._width = wFrame._width;
        frame._height = wFrame._height;
        frame._stride = wFrame._stride;
        frame._time = wFrame._time;
        // m_ffmFrame 视频帧缓存
        frame._frame = m_ffmFrame;
        frame._index = frameIndex++;
        //拷贝到 m_ffmFrame 视频帧缓存
        memcpy(frame._frame, wFrame._frame, static_cast<size_t>(size));
        m_waylandList.first()._frame = nullptr;
        //删队首视频帧 waylandFrame，未删空闲内存 waylandFrame::_frame，只删索引，不删内存空间
        m_waylandList.removeFirst();
        //回收空闲内存，重复使用
        m_freeList.append(wFrame._frame);
        // qDebug() << "获取视频帧";
        return true;
    }
}

bool WaylandIntegration::WaylandIntegrationPrivate::isWriteVideo()
{
    QMutexLocker locker(&m_mutex);
    if (Utils::isFFmpegEnv) {
        if (m_recordAdmin->m_writeFrameThread->bWriteFrame()) {
            return true;
        }
    } else {
        if (isGstWriteVideoFrame) {
            return true;
        }
    }
    return !m_waylandList.isEmpty();
}

bool WaylandIntegration::WaylandIntegrationPrivate::bGetFrame()
{
    QMutexLocker locker(&m_bGetFrameMutex);
    return m_bGetFrame;
}

void WaylandIntegration::WaylandIntegrationPrivate::setBGetFrame(bool bGetFrame)
{
    QMutexLocker locker(&m_bGetFrameMutex);
    m_bGetFrame = bGetFrame;
}

int WaylandIntegration::WaylandIntegrationPrivate::getPadStride(int width, int depth, int pad)
{
    int content = width * depth;
    if (pad == 1 || content % pad == 0) {
        return content;
    }
    return content + pad - content % pad;
}

void WaylandIntegration::WaylandIntegrationPrivate::copyBuffer(unsigned char *tmpDst,
                                                               unsigned char *tmpSrc,
                                                               const KWayland::Client::RemoteBuffer *rbuf)
{
    unsigned char *dst = tmpDst;
    unsigned char *buf = tmpSrc;
    int srcStride = getPadStride(rbuf->width(), 4, 32);
    qDebug() << "current strade" << srcStride;

    for (int i = 0; i < rbuf->height(); i++) {
        memcpy(dst, buf, srcStride);
        buf += rbuf->stride();
        dst += srcStride;
    }
}
