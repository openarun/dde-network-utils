/*
 * Copyright (C) 2011 ~ 2019 Deepin Technology Co., Ltd.
 *
 * Author:     listenerri <listenerri@gmail.com>
 *
 * Maintainer: listenerri <listenerri@gmail.com>
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

#include "connectivitychecker.h"

#include <QEventLoop>
#include <QNetworkReply>
#include <QScopedPointer>
#include <QGSettings>
#include <QTimer>

//当没有进行配置的时候, 则访问我们官网
static const QStringList CheckUrls {
    "https://www.uniontech.com",
};

#define TIMERINTERVAL (60 * 1000) // 一分钟
#define TIMEOUT (30 * 1000) // 30s超时

using namespace dde::network;

ConnectivityChecker::ConnectivityChecker(QObject *parent)
    : QObject(parent)
    , m_count(0)
{
    if (QGSettings::isSchemaInstalled("com.deepin.dde.network-utils")) {
        m_settings = new QGSettings("com.deepin.dde.network-utils", "/com/deepin/dde/network-utils/", this);
        m_checkUrls = m_settings->get("network-checker-urls").toStringList();
        connect(m_settings, &QGSettings::changed, [ = ] (const QString key) {
            if (key == "network-checker-urls") {
                m_checkUrls = m_settings->get("network-checker-urls").toStringList();
            }
        });
    }

    if (m_checkUrls.isEmpty()) {
        m_checkUrls = CheckUrls;
    }

    m_checkConnectivityTimer =  new QTimer(this);
    m_checkConnectivityTimer->setInterval(TIMERINTERVAL);

    connect(m_checkConnectivityTimer, &QTimer::timeout, this,
               &ConnectivityChecker::startCheck);

    m_checkConnectivityTimer->start();
}

void ConnectivityChecker::startCheck()
{
    if (m_checkUrls.size() <= m_count) {
        Q_EMIT checkFinished(false);
        m_count = 0;
        return;
    }

    auto reply(nam.head(QNetworkRequest(QUrl(m_checkUrls[m_count]))));
    qDebug() << "Check connectivity using url:" << m_checkUrls[m_count];

    // Do not use waitForReadyRead to block thread,
    // the QNetworkReply is not implement this virtual method
    // and it will just return false immediately
    //        reply->waitForReadyRead(-1);

    // Blocking, about 30 second to timeout
    QTimer *timer = new QTimer(this);
    timer->setSingleShot(true);

    connect(timer, &QTimer::timeout, this, [=] {
        reply->close();
        timer->deleteLater();
        reply->deleteLater();
        m_count++;
        this->startCheck();
    });
    connect(reply, &QNetworkReply::finished, this, [=] {
        int stateCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() == QNetworkReply::NoError && (stateCode >= 200 && stateCode <= 206)) {
            qDebug() << "Connected to url:" << m_checkUrls.first();
            Q_EMIT checkFinished(true);
            m_count = 0;
            reply->deleteLater();
            timer->stop();
            timer->deleteLater();
        } else {
            QMetaObject::invokeMethod(timer, "timeout");
        }
    });
    timer->start(TIMEOUT);
}
