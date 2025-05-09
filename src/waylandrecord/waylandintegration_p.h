// Copyright © 2018 Red Hat, Inc
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef XDG_DESKTOP_PORTAL_KDE_WAYLAND_INTEGRATION_P_H
#define XDG_DESKTOP_PORTAL_KDE_WAYLAND_INTEGRATION_P_H

#include "waylandintegration.h"
#include <QDateTime>
#include <QObject>
#include <QMap>
#include <QImage>
#include <gbm.h>
#include <epoxy/egl.h>
#include <epoxy/gl.h>
#include <QMutex>
#include <EGL/egl.h>

class RecordAdmin;
class ScreenCastStream;

namespace KWayland {
namespace Client {
class ConnectionThread;
class EventQueue;
class OutputDevice;
class Registry;
class RemoteAccessManager;
class RemoteBuffer;
class Output;
}
}

namespace WaylandIntegration {
class WaylandIntegrationPrivate : public WaylandIntegration::WaylandIntegration
{
    Q_OBJECT

public:
    struct EglStruct {
        EGLDisplay dpy;
        EGLContext ctx;
        EGLConfig conf;
    };
    //缓存帧
    struct waylandFrame {
        //时间戳
        int64_t _time;
        //索引
        int _index;
        int _width;
        int _height;
        int _stride;
        unsigned char *_frame;
    };

    struct FrameData {
        quint32 _width;
        quint32 _height;
        quint32 _stride;
        unsigned char *_frame;
        QImage::Format _format;
        QRect _rect ;
        bool _flag;
    };

    typedef struct {
        uint nodeId;
        QVariantMap map;
    } Stream;
    typedef QList<Stream> Streams;

    WaylandIntegrationPrivate();
    ~WaylandIntegrationPrivate();

    void initWayland(QStringList list);
    void initWayland(QStringList list, GstRecordX *gstRecord);

    /**
     * @brief 初始化wayland链接
     */
    void initConnectWayland();
    /**
     * @brief 从电脑/sys/class/dmi/id/board_vendor文件获取，电脑的厂商
     * @return 0:非hw电脑 1:hw电脑
     */
    int getBoardVendorType();
    /**
     * @brief 根据命令行 dmidecode -s system-product-name|awk '{print SNF}' 返回的结果判断是否是华为电脑
     * @return 0:非hw电脑 1:hw电脑
     */
    int getProductType();

    bool isPangu();

    bool isEGLInitialized() const;

    void bindOutput(int outputName, int outputVersion);
    //bool startStreaming(const WaylandOutput &output);
    void stopStreaming();
    QMap<quint32, WaylandOutput> screens();

    /**
     * @brief stopVideoRecord:停止获取视频流
     * @return
     */
    bool stopVideoRecord();
    inline bool writeFrameToStream();
    //录屏管理器
    RecordAdmin *m_recordAdmin;
    //是否初始化录屏管理
    bool m_bInitRecordAdmin;

    //WriteFrameThread *m_writeFrameThread;
    //     pthread_t m_writeFrameThread;
signals:
    /**
     * @brief 是否是像服务器请求的最后一帧画面
     */
    void lastFrame();
protected Q_SLOTS:
    void addOutput(quint32 name, quint32 version);
    void removeOutput(quint32 name);
    void onDeviceChanged(quint32 name, quint32 version);
    /**
     * @brief 通过mmap的方式获取视频画面帧
     * @param rbuf
     */
    void processBuffer(const KWayland::Client::RemoteBuffer *rbuf, const QRect rect);
    void processBufferHw(const KWayland::Client::RemoteBuffer *rbuf, const QRect rect,int screenId = 0);

    /**
     * @brief copyScreenData 拷贝数据，当远程buffer没有数据传过来时，调用此接口，将上一次的屏幕数据做为当前的屏幕数据
     * @param screenId
     */
    void copyScreenData(int screenId);

    /**
     * @brief getImageFormat 根据wayland客户端bufferReady给过来的像素格式，转成QImage的格式
     * @param format
     * @return QImage的格式
     */
    QImage::Format getImageFormat(quint32 format);

    /**
     * @brief 此接口为了解决x86架构录屏mmap失败及花屏问题
     * @param rbuf
     */
    void processBufferX86(const KWayland::Client::RemoteBuffer *rbuf, const QRect rect);
    /**
     * @brief 通过线程每30ms钟向数据池中取出一张图片添加到环形缓冲区，以便后续视频编码
     */
    void appendFrameToList();

    /**
     * @brief 通过线程循环向gstreamer管道写入视频帧数据
     */
    void gstWriteVideoFrame();

    /**
        * @brief 从wayland客户端获取当前屏幕的截图
        * @param fd
        * @param width
        * @param height
        * @param stride
        * @param format
        * @return
        */
    QImage getImage(int32_t fd, uint32_t width, uint32_t height, uint32_t stride, uint32_t format);

    /**
     * @brief 安装注册wayland客户服务
     */
    void setupRegistry();

    /**
     * @brief 初始化EGL
     */
    void initEgl();

private:
    /**
     * @brief appendBuffer:存视频帧
     * @param frame:视频帧
     * @param width:视频帧宽
     * @param height:视频帧高
     * @param stride:通道数
     * @param time:时间戳
     */
    void appendBuffer(unsigned char *frame, int width, int height, int stride, int64_t time);
    /**
     * @brief initScreenFrameBuffer 初始化屏幕数据数组
     */
    void initScreenFrameBuffer();
    /**
     * @brief appendRemoteBuffer 在线程中拷贝远程buffer（remoteaccess传过来的屏幕buffer）的数据
     */
    void appendRemoteBuffer();
public:
    /**
     * @ 内存由getFrame函数内部申请
     * @ 保存视频帧之后，无需：delete <unsigned char* frame>
     * @brief getFrame:获取帧
     * @param frame:视频帧
     * @return
     */
    bool getFrame(waylandFrame &frame);

    bool isWriteVideo();

    bool bGetFrame();
    void setBGetFrame(bool bGetFrame);

    int m_fps = 0;
    /**
     * @brief 从数据池取取数据的线程的开关。当数据池有数据时被启动
     */
    bool m_appendFrameToListFlag = false;

    /**
     * @brief getPadStride
     * @param width: rbuf width
     * @param depth: color depth
     * @param pad
     * @return correct pad
     */
    int getPadStride(int width, int depth, int pad);

    /**
     * @brief copyBuffer
     * @param tmpDst: app cache
     * @param tmpSrc: kde cache
     * @param rbuf
     */
    void copyBuffer(unsigned char * tmpDst, unsigned char * tmpSrc, const KWayland::Client::RemoteBuffer *rbuf);
private:
    /**
     * @brief 是否是hw电脑
     */
    int m_boardVendorType = 0;
    //缓存帧容量
    int m_bufferSize;
    int m_width = 0;
    int m_height = 0;
    //通道数
    int m_stride = 0;
    bool m_bInit;
    QMutex m_mutex;
    //wayland缓冲区
    QList<waylandFrame> m_waylandList;
    //空闲内存
    QList<unsigned char *> m_freeList;

    QPair<qint64, QImage> m_curNewImage;
    FrameData m_curNewImageData;



    //ffmpeg视频帧
    unsigned char *m_ffmFrame;
    //起始时间戳
    int64_t frameStartTime = 0;
    //是否获取视频帧
    bool m_bGetFrame;
    QMutex m_bGetFrameMutex;
    static int frameIndex;

    bool m_eglInitialized;
    bool m_streamingEnabled = false;
    bool m_registryInitialized;

    quint32 m_output = 0;
    QDateTime m_lastFrameTime;
    //ScreenCastStream *m_stream;

    QThread *m_thread = nullptr;

    QMap<quint32, WaylandOutput> m_outputMap;
    QList<KWayland::Client::Output *> m_bindOutputs;

    KWayland::Client::ConnectionThread *m_connection;
    KWayland::Client::EventQueue *m_queue;
    KWayland::Client::Registry *m_registry;
    KWayland::Client::RemoteAccessManager *m_remoteAccessManager;

    qint32 m_drmFd = 0; // for GBM buffer mmap
    gbm_device *m_gbmDevice = nullptr; // for passed GBM buffer retrieval
    pthread_t  m_initRecordThread;

    /**
     * @brief 自定义egl的结构体
     */
    struct EglStruct m_eglstruct;
    QMutex m_bGetScreenImageMutex;
    QMap<QString, QRect> m_screenId2Point;
    //双屏情况
    QVector<QPair<QRect, QImage>> m_ScreenDateBuf;
    QVector<QPair<QRect, QImage>> m_curNewImageScreen;
    //hw双屏
    unsigned char *m_firstScreenData = nullptr;
    unsigned char *m_secondScreenData = nullptr;

    //第一次拷贝数据的时候需要开辟内存空间
    bool m_isExistFirstScreenData = false;
    bool m_isExistSecondScreenData = false;
    FrameData m_ScreenDateBufFrames[2];
    FrameData m_curNewImageScreenFrames[2];

    /**
     * @brief m_lastScreenData 上一帧数据
     */
    FrameData m_lastScreenDatas[2];

    QSize m_screenSize;
    int m_screenCount;

    /**
     * @brief 是否写入Gstreamer视频帧画面
     */
    bool isGstWriteVideoFrame = false;

    GstRecordX *m_gstRecordX;
    /**
     * @brief m_isReleaseCurrentBuffer 当前远程buffer是否释放
     */
    bool m_isReleaseCurrentBuffer = true;

    /**
     * @brief m_isAppendRemoteBuffer 拷贝远程buffer（remoteaccess传过来的屏幕buffer）的线程是否继续执行
     */
    bool m_isAppendRemoteBuffer = false;
    /**
     * @brief m_currentScreenRect 当前屏幕的大小
     */
    QRect m_currentScreenRects[2];

    /**
     * @brief m_currentScreenBuf 当前屏幕的远程buffer（remoteaccess传过来的屏幕buffer）
     */
    KWayland::Client::RemoteBuffer * m_currentScreenBufs[2];

    /**
     * @brief m_isScreenExtension 当前屏幕模式是否是扩展模式
     */
    bool m_isScreenExtension = true;
};

}

#endif // XDG_DESKTOP_PORTAL_KDE_WAYLAND_INTEGRATION_P_H


