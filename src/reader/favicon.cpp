#include "favicon.h"

Favicon::Favicon(QObject *parent, ForumSubscription *sub) :
    QObject(parent) {
    subscription = sub;
    currentProgress = 0;
    reloading = false;
    connect(subscription->parserEngine(), SIGNAL(statusChanged(ForumSubscription*,bool,float)),
            this, SLOT(engineStatusChanged(ForumSubscription*,bool,float)));
    connect(&blinkTimer, SIGNAL(timeout()), this, SLOT(update()));
    blinkTimer.setInterval(100);
    blinkTimer.setSingleShot(false);
}

Favicon::~Favicon() {
}

void Favicon::fetchIcon(const QUrl &url, const QPixmap &alt) {
    //    qDebug() << Q_FUNC_INFO << "Fetching icon " << url.toString() << " for " << engine->subscription()->toString();
    currentpic = alt;
    blinkAngle = 0;
    QNetworkRequest req(url);
    connect(&nam, SIGNAL(finished(QNetworkReply*)), this, SLOT(replyReceived(QNetworkReply*)), Qt::UniqueConnection);
    nam.get(req);
    emit iconChanged(subscription, currentpic);
    update();
}

void Favicon::replyReceived(QNetworkReply *reply) {
    disconnect(&nam, SIGNAL(finished(QNetworkReply*)), this, SLOT(replyReceived(QNetworkReply*)));
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray bytes = reply->readAll();
        QPixmap iconPixmap;
        iconPixmap.loadFromData(bytes);
        if(!iconPixmap.isNull()) {
            currentpic = iconPixmap;
            emit iconChanged(subscription, currentpic);
        }
    }
    reply->deleteLater();
}

void Favicon::update() {
    QPixmap outPic(currentpic);
    blinkAngle -= 0.05;

    QPainter painter(&outPic);

    QRect rect(0, 0, outPic.width(), outPic.height());
    painter.setBrush(QColor(0, 0, 0, 64));
    rect.setRect(0, 0, outPic.height(), outPic.width());
    painter.drawRects(&rect, 1);

    painter.setPen(QColor(255, 255, 255, 64));
    painter.setBrush(QColor(255, 255, 255, 128));
    painter.drawPie(rect, blinkAngle * 5760, 1500);
    painter.setPen(QColor(0, 0, 0, 64));
    painter.setBrush(QColor(0, 0, 0, 128));
    painter.drawPie(rect, blinkAngle * 5760 - (5760/2), 1500);

    emit iconChanged(subscription, QIcon(outPic));
}

void Favicon::engineStatusChanged(ForumSubscription* fs,bool reloading,float progress) {
    if(reloading) {
        update();
        blinkTimer.start();
        //        emit iconChanged(subscription, QIcon(":data/view-refresh.png"));
    } else {
        blinkAngle = 0;
        blinkTimer.stop();
        emit iconChanged(subscription, currentpic);
    }
}
/*
void Favicon::setReloading(bool rel, float progress) {
    return;
    if (rel != reloading || currentProgress != progress) {
        currentProgress = progress;
        if (currentProgress > 1)
            currentProgress = 1;

        reloading = rel;
        update();
    }
}
*/
