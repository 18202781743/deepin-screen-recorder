// SPDX-FileCopyrightText: 2020-2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef RECORDTIME_H
#define RECORDTIME_H

#include "recordiconwidget.h"
#include "quickpanelwidget.h"
#include "tipswidget.h"

#include <QtDBus/QtDBus>

#include <dde-dock/constants.h>
#if defined(DOCK_API_VERSION) && (DOCK_API_VERSION >= DOCK_API_VERSION_CHECK(2, 0, 0))
#include <dde-dock/pluginsiteminterface_v2.h>
#else
#include <dde-dock/pluginsiteminterface.h>
#endif

#include <QScopedPointer>

#if defined(DOCK_API_VERSION) && (DOCK_API_VERSION >= DOCK_API_VERSION_CHECK(2, 0, 0))
class ShotStartRecordPlugin : public QObject, PluginsItemInterfaceV2
#else
class ShotStartRecordPlugin : public QObject, PluginsItemInterface
#endif
{
    Q_OBJECT
    Q_INTERFACES(PluginsItemInterface)
    Q_PLUGIN_METADATA(IID "com.deepin.dock.shotstart" FILE "shotstartrecord.json")
    Q_CLASSINFO("D-Bus Interface", "com.deepin.ShotRecorder.Recorder.PanelStatus")

public:
    explicit ShotStartRecordPlugin(QObject *parent = nullptr);
    ~ShotStartRecordPlugin() override;

    const QString pluginName() const override;
    const QString pluginDisplayName() const override;
    void init(PluginProxyInterface *proxyInter) override;

    // 返回插件是否允许被禁用
    bool pluginIsAllowDisable() override { return true; }
    bool pluginIsDisable() override;
    void pluginStateSwitched() override;

#if defined(DOCK_API_VERSION) && (DOCK_API_VERSION >= DOCK_API_VERSION_CHECK(2, 0, 0))
    Dock::PluginFlags flags() const override;
#endif

    /**
     * Dock::Type_Quick=0x02           插件类型-快捷插件区;
     * Dock::Quick_Panel_Single=0x40   当插件类型为Common时,快捷插件区域只有一列的那种插件;
     * Dock::Attribute_Normal=0xe00    普通插件;
     */
    Q_PROPERTY(int pluginFlags READ pluginFlags)
    int pluginFlags() const { return 0x02 | 0x40 | 0xe00; }
    PluginSizePolicy pluginSizePolicy() const override { return PluginsItemInterface::Custom; }

    QWidget *itemWidget(const QString &itemKey) override;
    QWidget *itemTipsWidget(const QString &itemKey) override;

    // 托盘图标排序
    int itemSortKey(const QString &itemKey) override;
    void setSortKey(const QString &itemKey, const int order) override;

    // 鼠标单击执行命令
    const QString itemCommand(const QString &itemKey) override;

    // 右键菜单
    const QString itemContextMenu(const QString &itemKey) override;
    void invokedMenuItem(const QString &itemKey, const QString &menuId, const bool checked) override;

public Q_SLOTS:
    Q_SCRIPTABLE bool onStart();
    Q_SCRIPTABLE void onStop();
    Q_SCRIPTABLE void onRecording();
    Q_SCRIPTABLE void onPause();

private:
    void onClickQuickPanel();
    void setTrayIconVisible(bool visible);
    bool getTrayIconVisible();

private:
    QScopedPointer<RecordIconWidget> m_iconWidget;       // 任务栏图标
    QScopedPointer<QuickPanelWidget> m_quickPanelWidget; // 快捷面板
    QScopedPointer<TipsWidget> m_tipsWidget;
    bool m_isRecording;                // true:正在录屏 false:未启动录屏
    QTimer *m_checkTimer;              // 此定时器的作用为每隔1秒检查下截图录屏是否还在运行中。避免截图录屏崩溃后导致本插件还在执行
    int m_nextCount = 0;               // 用来判断录屏是否正在进行中
    int m_count = 0;                   // 用来判断录屏是否正在进行中
    bool m_bDockQuickPanel;            // 兼容性适配，老版的dock不支持快捷面板
    bool m_bPreviousIsVisable = false; // 记录前置状态是否为禁止使能状态
};

// from dde-shell dockiteminfo.h
struct DockItemInfo
{
    QString name;
    QString displayName;
    QString itemKey;
    QString settingKey;
    QString dccIcon;
    bool visible;
};
Q_DECLARE_METATYPE(DockItemInfo)

typedef QList<DockItemInfo> DockItemInfos;
Q_DECLARE_METATYPE(DockItemInfos)

#endif // RECORDTIME_H
