/*
 * Copyright (C) 2020 ~ 2021 Uniontech Software Technology Co.,Ltd.
 *
 * Author:     Hou Lei <houlei@uniontech.com>
 *
 * Maintainer: Liu Zheng <liuzheng@uniontech.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include "utils/scrollScreenshot.h"
#include "record_process.h"
#include "countdown_tooltip.h"
#include "button_feedback.h"
#include "show_buttons.h"
#include "widgets/shapeswidget.h"
#include "widgets/toptips.h"
#include "widgets/toolbar.h"
#include "widgets/sidebar.h"
#include "widgets/keybuttonwidget.h"
#include "widgets/zoomIndicator.h"
#include "widgets/camerawidget.h"
#include "widgets/filter.h"
#include "widgets/scrollshottip.h"
#include "widgets/previewwidget.h"
#include "utils/saveutils.h"
#include "utils/voicevolumewatcher.h"
#include "utils/camerawatcher.h"
#include "utils/screengrabber.h"
#include "dbusinterface/dbuscontrolcenter.h"
#include "dbusinterface/dbusnotify.h"
#include "dbusinterface/dbuszone.h"
#include "RecorderRegionShow.h"
#include "event_monitor.h"
#include "recordertablet.h"
#include "dbusinterface/ocrinterface.h"

#include <DWindowManagerHelper>
#include <DDialog>
#include <DWidget>

#include <QApplication>
#include <QDesktopWidget>
#include <QObject>
#include <QPainter>
#include <QSystemTrayIcon>
#include <QVBoxLayout>
#include <QTimer>

#undef Bool

DGUI_USE_NAMESPACE
DWIDGET_USE_NAMESPACE
//DWM_USE_NAMESPACE

class MainWindow : public DWidget
{
    Q_OBJECT

//    Q_CLASSINFO("D-Bus Interface", "com.deepin.ScreenRecorder")

    //static const int CURSOR_BOUND;
    static const int RECORD_MIN_SIZE;
    static const int RECORD_MIN_HEIGHT;
    static const int RECORD_MIN_SHOT_SIZE;
    static const int DRAG_POINT_RADIUS;

    /**
     * @brief 录屏开始按钮未点击的状态
     */
    static const int RECORD_BUTTON_NORMAL;
    /**
     * @brief 录屏开始按钮点击后3秒倒计时的状态
     */
    static const int RECORD_BUTTON_WAIT;
    /**
     * @brief 录屏按钮点击后正在录屏的状态
     */
    static const int RECORD_BUTTON_RECORDING;
    /**
     * @brief 录屏按钮点击后录屏结束保存时的状态
     */
    static const int RECORD_BUTTON_SAVEING;

    static const int ACTION_MOVE;
    static const int ACTION_RESIZE_TOP_LEFT;
    static const int ACTION_RESIZE_TOP_RIGHT;
    static const int ACTION_RESIZE_BOTTOM_LEFT;
    static const int ACTION_RESIZE_BOTTOM_RIGHT;
    static const int ACTION_RESIZE_TOP;
    static const int ACTION_RESIZE_BOTTOM;
    static const int ACTION_RESIZE_LEFT;
    static const int ACTION_RESIZE_RIGHT;

    static const int RECORD_OPTIONAL_PADDING;

    static const int CAMERA_WIDGET_MAX_WIDTH;
    static const int CAMERA_WIDGET_MAX_HEIGHT;
    static const int CAMERA_WIDGET_MIN_WIDTH;
    static const int CAMERA_WIDGET_MIN_HEIGHT;

    enum status {
        record = 0,
        shot,
        scrollshot
    };
    //滚动截图的滚动模式
    enum ScrollShotType {
        AutoScroll = 0, //自动滚动
        ManualScroll,  //手动滚动
        Unknow
    };

public:
    explicit MainWindow(DWidget *parent = nullptr);
    ~MainWindow()
    {
        if (m_pVoiceVolumeWatcher) {
            m_pVoiceVolumeWatcher->setWatch(false);
            //run函数里面有sleep,所以此处加terminate
#ifdef __x86_64__
            m_pVoiceVolumeWatcher->terminate();
#else
            m_pVoiceVolumeWatcher->exit();
#endif
            if (!QSysInfo::currentCpuArchitecture().startsWith("mips")) {
                m_pVoiceVolumeWatcher->wait();
                m_pVoiceVolumeWatcher = nullptr;
            }
        }
        if (m_pCameraWatcher) {
            m_pCameraWatcher->setWatch(false);
#ifdef __x86_64__
            m_pCameraWatcher->terminate();
#else
            m_pCameraWatcher->exit();
#endif
            if (!QSysInfo::currentCpuArchitecture().startsWith("mips")) {
                m_pCameraWatcher->wait();
                m_pCameraWatcher = nullptr;
            }
        }
#ifndef __mips__

        if (m_pScreenCaptureEvent && m_isZhaoxin == false) {
            m_pScreenCaptureEvent->terminate();
            m_pScreenCaptureEvent->wait();
            delete m_pScreenCaptureEvent;
            m_pScreenCaptureEvent = nullptr;
        }
#endif

        if (m_showButtons) {
            //退出进程，最好不用deleteLater，因为有可能等不到下一次事件循环，导致资源不能释放
            delete m_showButtons;
            m_showButtons = nullptr;
        }
        if (countdownTooltip) {
            delete countdownTooltip;
            countdownTooltip = nullptr;
        }
        if (buttonFeedback) {
            delete buttonFeedback;
            buttonFeedback = nullptr;
        }
        if (m_shapesWidget) {
            delete m_shapesWidget;
            m_shapesWidget = nullptr;
        }
        if (m_sizeTips) {
            delete m_sizeTips;
            m_sizeTips = nullptr;
        }
        if (m_toolBar) {
            delete m_toolBar;
            m_toolBar = nullptr;
        }
        if (m_zoomIndicator) {
            delete m_zoomIndicator;
            m_zoomIndicator = nullptr;
        }
        if (m_menuController) {
            delete m_menuController;
            m_menuController = nullptr;
        }
        if (m_cameraWidget) {
            delete m_cameraWidget;
            m_cameraWidget = nullptr;
        }
        if (m_tabletRecorderHandle) {
            delete m_tabletRecorderHandle;
            m_tabletRecorderHandle = nullptr;
        }
        if (m_sideBar) {
            delete m_sideBar;
            m_sideBar = nullptr;
        }
        if (m_pRecorderRegion) {
            delete m_pRecorderRegion;
            m_pRecorderRegion = nullptr;
        }
        if (m_scrollShotTip) {
            delete m_scrollShotTip;
            m_scrollShotTip = nullptr;
        }
//        if (m_scrollShot) {
//            delete m_scrollShot;
//            m_scrollShot = nullptr;
//        }
        if (m_previewWidget) {
            delete m_previewWidget;
            m_previewWidget = nullptr;
        }
//        if (m_ocrInterface) {
//            delete m_ocrInterface;
//            m_ocrInterface = nullptr;
//        }
//        if (m_CursorImage) {
//            free(m_CursorImage);
//            m_CursorImage = nullptr;
//        }
        for (int i = 0; i < m_keyButtonList.count(); i++) {
            delete m_keyButtonList.at(i);
        }
        m_keyButtonList.clear();

        //以前的流程没执行到此处，没暴露延迟500ms的问题，以前的无用代码
        //QThread::currentThread()->msleep(500);
    }

    enum ShotMouseStatus {
        Normal,
        Wait,
        Shoting,
    };
    struct ScreenInfo {
        int x;
        int y;
        int width;
        int height;
        QString name;
        ~ScreenInfo() {}
    };
    // Split attributes and resource for speed up start.
    void initAttributes();
    void initResource();
    void initScreenShot();
    void initScreenRecorder();
    /**
     * @brief 初始化滚动截图
     */
    void initScrollShot();

    /**
     * @brief 初始化应用内快捷键
     */
    void initShortcut();
    void initLaunchMode(const QString &launchMode);
    //void delayScreenshot(double num);
    void fullScreenshot();

    void topWindow();
    void savePath(const QString &path);
    void startScreenshotFor3rd(const QString &path);
    void noNotify();
    void sendSavingNotify();
    // 强制录屏保存退出通知,3D->2D模式
    void forciblySavingNotify();

signals:
    void releaseEvent();
    void hideScreenshotUI();
    void saveActionTriggered();
    void unDo();
    void unDoAll();
    void deleteShapes();
    void changeMicrophoneSelectEvent(bool checked);
    void changeSystemAudioSelectEvent(bool checked);

public slots:
    void onExit();
    void startRecord();

    //void flashTrayIcon();
    //void iconActivated(QSystemTrayIcon::ActivationReason reason);
    void shotCurrentImg();
    void addCursorToImage();
    void shotFullScreen(bool isFull = false);
    void onHelp();

    Q_SCRIPTABLE void stopRecord();
    void startCountdown();
    void showPressFeedback(int x, int y);
    void showDragFeedback(int x, int y);
    void showReleaseFeedback(int x, int y);
    void responseEsc();
    void compositeChanged();
    void updateToolBarPos();
    void updateSideBarPos();
    //void updateRecordButtonPos();
    //void updateShotButtonPos();
    void updateCameraWidgetPos();
    /**
     * @brief 切换截图功能或者录屏功能
     * @param shapeType : "record" or "shot"
     */
    void changeFunctionButton(QString type);
    void showKeyBoardButtons(const QString &key);
    void changeKeyBoardShowEvent(bool checked);
    void changeMouseShowEvent(bool checked);
    void changeCameraSelectEvent(bool checked);
    void updateMultiKeyBoardPos();
    /**
     * @brief 改变截图工具栏操作入口
     * @param 工具栏点击的按钮类型，例如：ocr、pen等
     */
    void changeShotToolEvent(const QString &func);
    void saveScreenShot();
    bool saveAction(const QPixmap &pix);
    void sendNotify(SaveAction saveAction, QString saveFilePath, const bool succeed);
    void reloadImage(QString effect);
    void shotImgWidthEffect();
    //void changeArrowAndLineEvent(int line);
    void exitApp();
    void onViewShortcut();
    void shapeClickedSlot(QString shape);
    //void on_CheckRecodeCouldUse(bool canUse);
    void on_CheckVideoCouldUse(bool canUse);
    void onShotKeyPressEvent(const unsigned char &keyCode);
    void onRecordKeyPressEvent(const unsigned char &keyCode);
    void tableRecordSet();

    /**
     * @brief 切换活动窗口
     */
    void onActivateWindow();

    /**
     * @brief 通过x11从底层获取鼠标拖动事件
     * @param 鼠标按下的x坐标
     * @param 鼠标按下的y坐标
     */
    void onMouseDrag(int x, int y);

    /**
     * @brief 通过x11从底层获取鼠标按压事件
     * @param 鼠标按下的x坐标
     * @param 鼠标按下的y坐标
     */
    void onMousePress(int x, int y);

    /**
     * @brief 通过x11从底层获取鼠标释放事件
     * @param x
     * @param y
     */
    void onMouseRelease(int x, int y);

    /**
     * @brief 通过x11从底层获取鼠标移动事件
     * @param 鼠标移动的x坐标
     * @param 鼠标移动的y坐标
     */
    void onMouseMove(int x, int y);

    /**
     * @brief 通过x11从底层滚动鼠标滚轮
     * @param direction 鼠标滚动的方向： 1：向上滚动； 0：向下滚动
     * @param 鼠标移动的x坐标
     * @param 鼠标移动的y坐标
    */
    void onMouseScroll(int mouseTime, int direction, int x, int y);

    /**
     * @brief 通过x11从底层获取键盘按下事件
     * @param keyCode: 按下的键盘按键代号
     */
    void onKeyboardPress(unsigned char keyCode);

    /**
     * @brief 通过x11从底层获取键盘释放事件
     * @param keyCode: 释放的键盘按键代号
     */
    void onKeyboardRelease(unsigned char keyCode);

    /**
     * @brief 滚动截图时通过x11获取鼠标按键操作
     * @param x
     * @param y
     */
    void scrollShotMouseClickEvent(int x, int y);

    /**
     * @brief 滚动截图时通过x11获取鼠标移动操作
     * @param x
     * @param y
     */
    void scrollShotMouseMoveEvent(int x, int y);
    /**
     * @brief 滚动截图时处理鼠标滚轮滚动
     * @param direction 鼠标滚动的方向： 1：向上滚动； 0：向下滚动
     * @param x 当前的x坐标
     * @param y 当前的y坐标
     */
    void scrollShotMouseScrollEvent(int mouseTime, int direction, int x, int y);

    /**
     * @brief 监听是否正在进行自动滚动
     * @param autoScrollFlag 进行自动滚动时,模拟滚动的操作会,进行次数加1
     */
    void onScrollShotCheckScrollType(int autoScrollFlag);

    /**
     * @brief 监听锁屏信号，滚动截图时锁屏进行暂停处理
     * @param msg
     */
    void onLockScreenEvent(QDBusMessage msg);

    /**
     * @brief 打开截图录屏帮助文档并定位到滚动截图
     */
    void onOpenScrollShotHelp();

    /**
     * @brief 自动调整捕捉区域的大小及位置
     */
    void onAdjustCaptureArea();

    /**
     * @brief 获取滚动截图拼接的状态
     * @param 拼接的状态
     * 1：拼接失败
     * 2：滚动到底部
     * 3：拼接截图到截图最大限度
     */
    void onScrollShotMerageImgState(PixMergeThread::MergeErrorValue state);

    /**
     * @brief initPadShot:初始化平板截图
     */
    void initPadShot();

    /**
     * @brief 退出截图录屏事件监控线程
     */
    void exitScreenCuptureEvent();
    void showPreviewWidgetImage(QImage img);//显示预览窗口和图片
protected:
    bool eventFilter(QObject *object, QEvent *event);
    int getAction(QEvent *event);
    void paintEvent(QPaintEvent *event);
    void resizeBottom(QMouseEvent *event);
    void resizeLeft(QMouseEvent *event);
    void resizeRight(QMouseEvent *event);
    void resizeTop(QMouseEvent *event);
    void updateCursor(QEvent *event);
    void setDragCursor();
    void resetCursor();
    void setFontSize(QPainter &painter, int textSize);
    void hideAllWidget();
    //void adjustLayout(QVBoxLayout *layout, int layoutWidth, int layoutHeight);
    void initShapeWidget(QString type);
    int getRecordInputType(bool selectedMic, bool selectedSystemAudio);
    void initBackground();
    QPixmap getPixmapofRect(const QRect &rect);
    bool saveImg(const QPixmap &pix, const QString &fileName, const char *format = nullptr);

    /**
     * @brief 初始化滚动截图时，显示滚动截图中的一些公共部件、例如工具栏、提示、图片大小
     */
    void showScrollShot();

    /**
     * @brief 根据工具栏获取滚动截图提示框的坐标
     * @return  提示框的位置
     */
    QPoint getScrollShotTipPosition();

    /**
     * @brief 开始自动滚动截图，只进入一次
     */
    void startAutoScrollShot();

    /**
     * @brief 暂停自动滚动截图
     */
    void pauseAutoScrollShot();

    /**
     * @brief 继续自动滚动截图
     */
    void continueAutoScrollShot();

    /**
     * @brief 开始手动滚动截图，只进入一次
     */
    void startManualScrollShot();

    /**
     * @brief 处理手动滚动截图逻辑
     * @param 鼠标滚动的方向
     */
    void handleManualScrollShot(int mouseTime, int direction);

    /**
     * @brief 滚动截图时鼠标穿透设置
     * 之所以需要单独用来设置，因为有些时候捕捉区域太大，
     * 工具栏在捕捉区域内部，需要将工具栏这片区域给排除掉
     */
    void setInputEvent();

    /**
     * @brief 滚动截图时取消捕捉区域的鼠标穿透
     * 当工具栏、保存按钮或者滚动截图提示按钮在 捕捉区域内部时，
     * 并且需要点击时，捕捉区域必须取消穿透状态才可以进行点击
     */
    void setCancelInputEvent();

    /**
     * @brief 显示可调整的捕捉区域大小及位置
     */
    void showAdjustArea();

    /**
     * @brief 滚动截图模式，抓取当前捕捉区域的图片，传递给滚动截图处理类进行图片的拼接
     * @param previewPostion 预览框相对于捕捉区域的位置
     * @param direction 滚动的方向
     */
    void scrollShotGrabPixmap(PreviewWidget::PostionStatus previewPostion, int direction, int mosueTime = 0);

    /**
     * @brief 判断工具栏是否在在捕捉区域内部
     * @return false: 不在  ； true: 在
     */
    bool isToolBarInShotArea();
private:
//    QList<WindowRect> windowRects;
    QList<QRect> windowRects;
    QList<QString> windowNames;
    ShowButtons *m_showButtons = nullptr;
    //QTimer *flashTrayIconTimer = nullptr;
    QRect screenRect;
    RecordProcess recordProcess;
    voiceVolumeWatcher *m_pVoiceVolumeWatcher = nullptr;
    CameraWatcher *m_pCameraWatcher = nullptr;
    ScreenGrabber m_screenGrabber;
//    VoiceRecordProcess voiceRecordProcess;
//    WindowRect rootWindowRect;
    QRect rootWindowRect;
    SaveAction m_saveIndex = SaveAction::SaveToClipboard;
    //m_saveFileName is the storage path of the screenshot image.
    QString m_saveFileName;

    bool drawDragPoint = false;
    bool isFirstDrag = false;
    bool isFirstMove = false;
    bool isFirstPressButton = false;
    bool isFirstReleaseButton = false;
    /**
     * @brief 是否按压按钮
     */
    bool isPressButton = false;
    /**
     * @brief 是否释放按钮
     */
    bool isReleaseButton = false;

    int dragAction = 0;
    int dragRecordHeight = 0;
    int dragRecordWidth = 0;
    int dragRecordX = -1;
    int dragRecordY = -1;
    int dragStartX = 0;
    int dragStartY = 0;

    int m_shotStatus = ShotMouseStatus::Normal; //
    int recordButtonStatus = RECORD_BUTTON_NORMAL;
    int recordHeight = 0;
    int recordWidth = 0;
    int recordX = 0;
    int recordY = 0;

    int flashTrayIconCounter = 0;
    QPixmap m_backgroundPixmap;
    QPixmap m_resultPixmap;
    QPixmap resizeHandleBigImg;
    //QPixmap resizeHandleSmallImg;

    QString selectAreaName = "";
    //dde-dock显示时长插件代替系统托盘
    //QSystemTrayIcon *trayIcon = nullptr;
    CountdownTooltip *countdownTooltip = nullptr;
    /**
     * @brief 滚动截图的提示
     */
    ScrollShotTip *m_scrollShotTip = nullptr;

    /**
     * @brief 初始化滚动截图时的第一张图
     */
    QPixmap m_firstScrollShotImg;

    /**
     * @brief 获取预览框相对于捕捉区域的位置
     */
    PreviewWidget::PostionStatus m_previewPostion = PreviewWidget::PostionStatus::RIGHT;

    /**
     * @brief 滚动截图状态
     * 0：停止(第一次进入)；
     * 1：自动滚动启动(第一次进入)；
     * 2：自动滚动继续；
     * 3：自动滚动(鼠标点击触发)暂停；
     * 4：自动滚动(鼠标移除捕捉区域)暂停；
     * 5：手动滚动启动(第一次进入)；
     * 6：自动滚动(手动滚动触发)暂停；
     *
     * 在捕捉区域内点击鼠标触发暂停，当鼠标移动到捕捉区域外，暂停改为鼠标移动触发，移回捕捉区域继续
     */
    int m_scrollShotStatus = 0;

    /**
     * @brief 自动滚动截图是否启动
     */
    bool m_isAutoScrollShotStart = false;

    /**
     * @brief 当前滚动截图的类型：0：自动滚动截图；  1：手动滚动截图；
     */
    int m_scrollShotType = ScrollShotType::AutoScroll;

    /**
     * @brief 自动滚动截图发出的前一次标志
     */
    int m_autoScrollFlagLast = 0;

    /**
     * @brief 自动滚动截图发出的后一次标志
     */
    int m_autoScrollFlagNext = 0;

    /**
     * @brief 是否显示自动调整的捕捉区域
     * false : 不显示；
     * true : 显示；
     */
    bool m_isAdjustArea = false;

    /**
     * @brief 滚动截图时,可自动调整的捕捉区域
     */
    QRect m_adjustArea ;

    /**
     * @brief 滚动截图模式下，是否进行鼠标左键点击的次数
     * 0：鼠标未点击
     * 1：鼠标点击一次
     * 2：鼠标点击两次
     * >2: 默认为两次
     * 点击间隔为0.5s，点击间隔的时间大于0.5s，不进行点击次数累加
     * scrollShotCDB
     */
    int m_scrollShotMouseClick = 0;

    /**
     * @brief 滚动截图是否出现错误，当出现错误时暂停继续滚动流程，直到错误被解决或者2s后错误消失
     * 默认为 false
     * 只当拼接图片出现自动调整捕捉区域异常时，此字段置为 true ;
     * 当点击自动调整捕捉区域时将 m_isErrorWithScrollShot 置为 false
     * 当自动调整捕捉区域提示 2s 后消失时将 m_isErrorWithScrollShot 置为 false
     */
    bool m_isErrorWithScrollShot = false;

    /**
      * @brief 滚动截图提示显示时间的定时器
      */
    QTimer *m_tipShowtimer = nullptr;

    ButtonFeedback *buttonFeedback = nullptr;
    /**
     * @brief 截图与录屏共用一个底层监听事件线程
     */
    EventMonitor *m_pScreenCaptureEvent = nullptr;

    DWindowManagerHelper *m_wmHelper;
    ShapesWidget *m_shapesWidget = nullptr;
    /**
     * @brief 左上角图片大小提示
     */
    TopTips *m_sizeTips = nullptr;
    /**
     * @brief 滚动截图时图片的大小
     */
    TopTips *m_scrollShotSizeTips = nullptr;
    ToolBar *m_toolBar = nullptr;
    QRect m_backgroundRect;
    //添加截屏和录屏的按钮
    /**
     * @brief 录屏开始按钮
     */
    //DPushButton *m_recordButton = nullptr;
    /**
     * @brief 截图保存按钮
     */
    //DPushButton *m_shotButton = nullptr;
    QList<KeyButtonWidget *> m_keyButtonList;

    /**
     * @brief 功能类型
     */
    int m_functionType = status::record;  //0: record, 1: shot , 2: scrollshot
    /**
     * @brief 键盘开关状态
     */
    bool m_keyBoardStatus = false; //false: keyBoard off, true:keyBoard On
    /**
     * @brief 鼠标开关状态
     */
    bool m_mouseStatus = false; //false: mouse check off, true:mouse check On
    /**
     * @brief 是否重绘录屏或截图按钮
     */
    bool m_repaintMainButton = false;//false: no need to repaint record button or shot button, true:...
    /**
     * @brief 是否重绘侧工具栏
     */
    bool m_repaintSideBar = false;   //false: no need to repaint sidebar, true:...
    int m_screenWidth = 0;  //屏幕宽度
    int m_screenHeight = 0; //屏幕高度
    SideBar *m_sideBar = nullptr; //截图功能侧边栏功能
    ZoomIndicator *m_zoomIndicator = nullptr;
    /**
     * @brief 是否打开截图工具中的形状编辑
     */
    bool m_isShapesWidgetExist = false;
    bool m_isSideBarInside = false;
    bool m_isToolBarInside = false;


    //截图功能使用的变量初始化
    MenuController *m_menuController = nullptr;
    bool m_noNotify = false;
    bool m_isShiftPressed = false;
    bool m_needSaveScreenshot = false;
    bool m_toolBarInit = false;
    bool m_sideBarInit = false;
    CameraWidget *m_cameraWidget = nullptr;
    bool m_selectedCamera = false;
    bool m_cameraOffFlag = false;
    bool m_launchWithRecordFunc = false;
    bool m_shotWithPath = false;
    int m_screenCount;
    QString m_shotSavePath;
    /**
     * @brief 保存截图的标志，进入保存截图时会置为1
     */
    int m_shotflag = 0;
    int m_firstShot = 0;
    bool m_isZhaoxin = false;
    QList<ScreenInfo> m_screenInfo;
    XFixesCursorImage *m_CursorImage = nullptr;
    QSize m_screenSize;
    RecorderRegionShow *m_pRecorderRegion = nullptr;
    /**
     * @brief 屏幕的缩放比例
     * 1.当屏幕缩放比例为1.25时，此时的屏幕实际大小为1536*864
     *   1.1.如果需将当前屏幕的点换算到1920*1080上需乘上m_pixelRatio
     *   1.2.如果需将1920*1080上的点换算到此屏幕应该除以m_pixelRatio
     */
    qreal m_pixelRatio = 1.0;
    bool m_hasComposite = true;
    bool m_initScreenShot;
    bool m_initScreenRecorder;
    RecorderTablet *m_tabletRecorderHandle = nullptr;
    int m_cursorBound;
    //ocr接口
    OcrInterface *m_ocrInterface;
    //预览窗口
    PreviewWidget *m_previewWidget = nullptr;
    /**
     * @brief 滚动截图图像拼接
     */
    ScrollScreenshot *m_scrollShot;


};

#endif //MAINWINDOW_H
