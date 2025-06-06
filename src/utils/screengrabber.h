// Copyright (C) 2020 ~ 2021 Uniontech Software Technology Co.,Ltd.
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SCREENGRABBER_H
#define SCREENGRABBER_H

#include <QObject>
#include <QPixmap>
#include <QRect>
#include <QList>

class QScreen;

class ScreenGrabber : public QObject
{
    Q_OBJECT
public:
    explicit ScreenGrabber(QObject *parent = nullptr);
    QPixmap grabEntireDesktop(bool &ok, const QRect &rect, const qreal devicePixelRatio);

private:
    QPixmap grabWaylandScreenshot(bool &ok, const QRect &rect, const qreal devicePixelRatio);
    QPixmap grabX11Screenshot(bool &ok, const QRect &rect);
    QList<QScreen*> findIntersectingScreens(const QRect &rect);
    QPixmap grabPrimaryScreenFallback(bool &ok, const QRect &rect);
    QPixmap grabSingleScreen(bool &ok, const QRect &rect, QScreen *screen);
    QPixmap grabMultipleScreens(bool &ok, const QRect &rect, const QList<QScreen*> &screens);
    QPixmap grabScreenFragment(QScreen *screen, const QRect &intersection);
};

#endif // SCREENGRABBER_H
