#include "siilihai.h"

Siilihai::Siilihai() :
	QObject(), fdb(this), pdb(this), syncmaster(this, fdb, protocol) {
    loginWizard = 0;
    mainWin = 0;
    parserMaker = 0;
    loginProgress = 0;
    groupSubscriptionDialog = 0;
    subscribeWizard = 0;
    endSyncDone = false;
}

void Siilihai::launchSiilihai() {
    currentState = state_started;
    mainWin = new MainWindow(pdb, fdb, &settings);
    bool firstRun = settings.value("first_run", true).toBool();

    settings.setValue("first_run", false);
    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(QDir::homePath() + DATABASE_FILE);
    if (!db.open()) {
        QMessageBox msgBox(mainWin);
        msgBox.setText("Error: Unable to open database.");
        msgBox.exec();
        haltSiilihai();
        return;
    }
    QString proxy = settings.value("preferences/http_proxy", "").toString();
    if (proxy.length() > 0) {
        QUrl proxyUrl = QUrl(proxy);
        if (proxyUrl.isValid()) {
            QNetworkProxy nproxy(QNetworkProxy::HttpProxy, proxyUrl.host(),
                                 proxyUrl.port(0));
            QNetworkProxy::setApplicationProxy(nproxy);
        } else {
            errorDialog("Warning: http proxy is not valid URL");
        }
    }
    baseUrl = settings.value("network/baseurl", BASEURL).toString();
    settingsChanged(false);
    connect(&syncmaster, SIGNAL(syncFinished(bool)), this, SLOT(syncFinished(bool)));
    protocol.setBaseURL(baseUrl);

    int mySchema = settings.value("forum_database_schema", 0).toInt();
    if(!firstRun && fdb.schemaVersion() != mySchema) {
        errorDialog("The database schema has been changed. Your forum database will be reset."
                    " Sorry. ");
        fdb.resetDatabase();
    }
    connect(&fdb, SIGNAL(subscriptionFound(ForumSubscription*)), this, SLOT(subscriptionFound(ForumSubscription*)));
    connect(&fdb, SIGNAL(subscriptionDeleted(ForumSubscription*)), this, SLOT(subscriptionDeleted(ForumSubscription*)));
    connect(&protocol, SIGNAL(getParserFinished(ForumParser)), this,
            SLOT(updateForumParser(ForumParser)));
    connect(&protocol, SIGNAL(userSettingsReceived(bool,UserSettings*)), this,
            SLOT(userSettingsReceived(bool,UserSettings*)));
    if(fdb.openDatabase()) {
        settings.setValue("forum_database_schema", fdb.schemaVersion());
    } else {
        errorDialog("Error opening Siilihai's database!\nSee console for details. Sorry.");
    }
    pdb.openDatabase();

#ifdef Q_WS_HILDON
    if(settings.value("firstrun", true).toBool()) {
        errorDialog("This is beta software\nMaemo version can't connect\n"
                    "to the Internet, so please do it manually before continuing.\n"
                    "The UI is also work in progress. Go to www.siilihai.com for details.");
        settings.setValue("firstrun", false);
    }
#endif
    if (settings.value("account/username", "").toString() == "") {
        loginWizard = new LoginWizard(mainWin, protocol, settings);
        connect(loginWizard, SIGNAL(finished(int)), this,
                SLOT(loginWizardFinished()));
    } else {
        launchMainWindow();
        tryLogin();
    }
}

void Siilihai::changeState(siilihai_states newState) {
    if(newState==state_offline) {
        qDebug() << Q_FUNC_INFO << "Offline";
        mainWin->setReaderReady(true, true);
    } else if(newState==state_startsyncing) {
        qDebug() << Q_FUNC_INFO << "Startsync";
        if(usettings.syncEnabled)
            syncmaster.startSync();
    } else if(newState==state_updating_parsers) {
        qDebug() << Q_FUNC_INFO << "Update parsers";
        if(parsersToUpdateLeft.isEmpty()) {
            changeState(state_ready);
            return;
        }
        protocol.getParser(parsersToUpdateLeft.takeFirst()->parser());
    } else if(newState==state_ready) {
        qDebug() << Q_FUNC_INFO << "Ready";
        if (loginProgress) {
            loginProgress->cancel();
            loginProgress->deleteLater();
            loginProgress = 0;
        }
        if (settings.value("preferences/update_automatically", false).toBool())
            updateClicked();
        mainWin->setReaderReady(true, false);
    } else if(newState==state_quitting) {
        qDebug() << Q_FUNC_INFO << "Quitting";

        if(currentState==state_ready)
            cancelClicked();

        mainWin->setReaderReady(false, false);
    }

    currentState = newState;
}

void Siilihai::tryLogin() {
    if (!loginProgress) {
        loginProgress = new QProgressDialog("Logging in..", "Cancel", 0, 100,
                                            mainWin);
        loginProgress->setWindowModality(Qt::WindowModal);
        loginProgress->setValue(0);
        connect(loginProgress, SIGNAL(canceled()), this, SLOT(haltSiilihai()));
    }
    connect(&protocol, SIGNAL(loginFinished(bool, QString,bool)), this,
            SLOT(loginFinished(bool, QString,bool)));
    protocol.login(settings.value("account/username", "").toString(),
                   settings.value("account/password", "").toString());
}

void Siilihai::haltSiilihai() {
    qDebug() << Q_FUNC_INFO;
    if(usettings.syncEnabled && !endSyncDone) {
        qDebug() << "Sync enabled - running end sync";
        changeState(state_endsync);
        syncmaster.endSync();
    } else {
        qDebug() << "Sync not enabled - quitting";
        changeState(state_quitting);
        settings.sync();
        mainWin->deleteLater();
        mainWin = 0;
        QCoreApplication::quit();
    }
}

void Siilihai::syncFinished(bool success){
    qDebug() << Q_FUNC_INFO << success;
    if (!success) {
        errorDialog("Syncing status to server failed. Please check network connection!");
    }
    if(currentState == state_startsyncing) {
        changeState(state_updating_parsers);
    } else if(currentState == state_endsync) {
        endSyncDone = true;
        haltSiilihai();
    }
}

void Siilihai::offlineModeSet(bool newOffline) {
    qDebug() << Q_FUNC_INFO << newOffline;
    if(currentState == state_offline && !newOffline) {
        tryLogin();
    } else if(currentState == state_ready && newOffline) {
        changeState(state_offline);
    } else {
        Q_ASSERT(false); // @todo NOT impossible state, fix later
    }
}

void Siilihai::loginFinished(bool success, QString motd, bool sync) {
    qDebug() << Q_FUNC_INFO << success;
    if (success) {
        connect(&protocol, SIGNAL(listSubscriptionsFinished(QList<int>)), this,
                SLOT(listSubscriptionsFinished(QList<int>)));
        connect(&protocol, SIGNAL(sendParserReportFinished(bool)), this,
                SLOT(sendParserReportFinished(bool)));
        connect(&protocol, SIGNAL(subscribeForumFinished(bool)), this, SLOT(subscribeForumFinished(bool)));

        usettings.syncEnabled = sync;
        qDebug() << "Server says user wants to sync: " << sync;
        settings.setValue("preferences/sync_enabled", usettings.syncEnabled);
        settings.sync();
        if (loginProgress)
            loginProgress->setValue(30);
        if(usettings.syncEnabled) {
            changeState(state_startsyncing);
        } else {
            changeState(state_updating_parsers);
        }
    } else {
        if (loginProgress) {
            loginProgress->cancel();
            loginProgress->deleteLater();
            loginProgress = 0;
        }
        QMessageBox msgBox(mainWin);
        msgBox.setModal(true);
        if (motd.length() > 0) {
            msgBox.setText(motd);
        } else {
            msgBox.setText(
                    "Error: Login failed. Check your username, password and network connection.\nWorking offline.");
        }
        msgBox.exec();
        changeState(state_offline);
    }
}

void Siilihai::listSubscriptionsFinished(QList<int> serversSubscriptions) {
    qDebug() << Q_FUNC_INFO << "count of subscribed forums " << serversSubscriptions.size();
    disconnect(&protocol, SIGNAL(listSubscriptionsFinished(QList<int>)), this,
               SLOT(listSubscriptionsFinished(QList<int>)));
    if (loginProgress)
        loginProgress->setValue(50);

    QList<ForumSubscription*> unsubscribedForums;
    foreach(ForumSubscription* sub, fdb.listSubscriptions()) {
        bool found = false;
        foreach(int serverSubscriptionId, serversSubscriptions) {
            qDebug() << "Server says: subscribed to " << serverSubscriptionId;
            if (serverSubscriptionId == sub->parser())
                found = true;
        }
        if (!found) {
            qDebug() << "Server says not subscribed to "
                    << sub->toString();
            unsubscribedForums.append(sub);
        }
    }
    foreach (ForumSubscription *sub, unsubscribedForums) {
        qDebug() << "Deleting forum " << sub->toString() << "as server says it's not subscribed";
        fdb.deleteForum(sub);
        pdb.deleteParser(sub->parser());
    }

    if (fdb.listSubscriptions().isEmpty()) { // Display subscribe dialog if none subscribed
        if(!usettings.syncEnabled) {
            subscribeForum();
            changeState(state_ready);
        }
    } else { // Update parser def's
        /*
        foreach(ForumSubscription *sub, fdb.listSubscriptions()) {
            parsersToUpdateLeft.append(sub);
            emit statusChanged(sub, true, -1);
        }
        */
        /*
        if (!parsersToUpdateLeft.isEmpty()) {
            protocol.getParser(parsersToUpdateLeft.at(0)->parser());
        } else {
            readerReady = true;
        }
        */
    }
}

// Stores the parser if subscribed to it and updates the engine
void Siilihai::updateForumParser(ForumParser parser) {
    qDebug() << Q_FUNC_INFO;
    if (parser.isSane()) {
        foreach(ForumSubscription *sub, engines.keys()) { // Find subscription that uses parser
            if (sub->parser() == parser.id) {
                pdb.storeParser(parser);
                engines[sub]->setParser(parser);
                emit statusChanged(sub, false, -1);
                if (parsersToUpdateLeft.size() < 2) {
                    if (loginProgress)
                        loginProgress->setValue(80);
                }

            } else {
                qDebug() << "WTF: Not subscribed to this forum, won't update.";
            }
        }
    }
    if(parsersToUpdateLeft.isEmpty()) {
        changeState(state_ready);
    } else {
        protocol.getParser(parsersToUpdateLeft.takeFirst()->parser());
    }
}


void Siilihai::subscribeForum() {
    subscribeWizard = new SubscribeWizard(mainWin, protocol, baseUrl);
    subscribeWizard->setModal(true);
    connect(subscribeWizard,
            SIGNAL(forumAdded(ForumParser, ForumSubscription*)), this,
            SLOT(forumAdded(ForumParser, ForumSubscription*)));
}

Siilihai::~Siilihai() {
    if (mainWin)
        mainWin->deleteLater();
    mainWin = 0;
}

void Siilihai::loginWizardFinished() {
    loginWizard->deleteLater();
    loginWizard = 0;
    if (settings.value("account/username", "").toString().length() == 0) {
        qDebug() << "Settings wizard failed, quitting.";
        haltSiilihai();
    } else {
        launchMainWindow();
        settingsChanged(false);
        loginFinished(true, QString(), usettings.syncEnabled);
    }
}

void Siilihai::launchMainWindow() {
    connect(mainWin, SIGNAL(subscribeForum()), this, SLOT(subscribeForum()));
    connect(mainWin, SIGNAL(unsubscribeForum(ForumSubscription*)), this,
            SLOT(showUnsubscribeForum(ForumSubscription*)));
    connect(mainWin, SIGNAL(updateClicked()), this, SLOT(updateClicked()));
    connect(mainWin, SIGNAL(updateClicked(ForumSubscription*,bool)), this,
            SLOT(updateClicked(ForumSubscription*,bool)));
    connect(mainWin, SIGNAL(cancelClicked()), this, SLOT(cancelClicked()));
    connect(mainWin, SIGNAL(groupSubscriptions(ForumSubscription*)), this,
            SLOT(showSubscribeGroup(ForumSubscription*)));
    connect(mainWin, SIGNAL(reportClicked(ForumSubscription*)), this, SLOT(reportClicked(ForumSubscription*)));
    connect(mainWin->threadList(),
            SIGNAL(messageSelected(ForumMessage*)), &fdb,
            SLOT(markMessageRead(ForumMessage*)));
    connect(mainWin, SIGNAL(launchParserMaker()), this,
            SLOT(launchParserMaker()));
    connect(mainWin, SIGNAL(offlineModeSet(bool)), this,
            SLOT(offlineModeSet(bool)));
    connect(mainWin, SIGNAL(haltRequest()), this,
            SLOT(haltSiilihai()));
    connect(mainWin, SIGNAL(settingsChanged(bool)), this, SLOT(settingsChanged(bool)));
    mainWin->setReaderReady(false, currentState==state_offline);
    mainWin->show();
}

void Siilihai::forumAdded(ForumParser fp, ForumSubscription *fs) {
    qDebug() << Q_FUNC_INFO << fp.toString() << fs->toString();
    if(subscribeWizard) {
        subscribeWizard->deleteLater();
        subscribeWizard = 0;
    }
    ForumSubscription *newSubscription = 0;
    if (!pdb.storeParser(fp) || !(newSubscription = fdb.addForum(fs))) {
        QMessageBox msgBox(mainWin);
        msgBox.setText(
                "Error: Unable to subscribe to forum. Are you already subscribed?");
        msgBox.exec();
    } else {
        protocol.subscribeForum(newSubscription);
        engines[newSubscription]->updateGroupList();
    }
}

void Siilihai::subscriptionFound(ForumSubscription *sub) {
    ParserEngine *pe = new ParserEngine(&fdb, this);
    ForumParser parser = pdb.getParser(sub->parser());
    pe->setParser(parser);
    pe->setSubscription(sub);
    engines[sub] = pe;
    //connect(pe, SIGNAL(groupListChanged(ForumSubscription*)), this,
     //       SLOT(showSubscribeGroup(ForumSubscription*)));
    connect(pe, SIGNAL(forumUpdated(ForumSubscription*)), this, SLOT(forumUpdated(ForumSubscription*)));
    connect(pe, SIGNAL(statusChanged(ForumSubscription*, bool, float)), this,
            SLOT(statusChanged(ForumSubscription*, bool, float)));
    connect(pe, SIGNAL(statusChanged(ForumSubscription*, bool, float)), mainWin,
            SLOT(setForumStatus(ForumSubscription*, bool, float)));
    connect(pe, SIGNAL(updateFailure(QString)), this,
            SLOT(errorDialog(QString)));
    parsersToUpdateLeft.append(sub);
}

void Siilihai::subscriptionDeleted(ForumSubscription *sub) {
    Q_ASSERT(engines.contains(sub));
    engines[sub]->cancelOperation();
    engines[sub]->deleteLater();
    engines.remove(sub);
}

void Siilihai::errorDialog(QString message) {
    QMessageBox msgBox(mainWin);
    msgBox.setModal(true);
    msgBox.setText(message);
    msgBox.exec();
}

void Siilihai::showSubscribeGroup(ForumSubscription* forum) {
    Q_ASSERT(forum);
    if (currentState == state_ready) {
        groupSubscriptionDialog = new GroupSubscriptionDialog(mainWin);
        groupSubscriptionDialog->setModal(false);
        groupSubscriptionDialog->setForum(&fdb, forum);
        connect(groupSubscriptionDialog, SIGNAL(finished(int)), this,
                SLOT(subscribeGroupDialogFinished()));
        groupSubscriptionDialog->exec();
    }
}

void Siilihai::subscribeGroupDialogFinished() {
    if (currentState == state_ready) {
        QList<ForumGroup*> newGroups = fdb.listGroups(groupSubscriptionDialog->subscription());
        protocol.subscribeGroups(newGroups);
        qDebug() << "SFD finished, updating list";
        updateClicked();
    }
    groupSubscriptionDialog->deleteLater();
    groupSubscriptionDialog = 0;
}

void Siilihai::forumUpdated(ForumSubscription* forum) {
    qDebug() << Q_FUNC_INFO << "Forum " << forum->toString() << " has been updated";
}

void Siilihai::updateClicked() {
    foreach(ParserEngine* engine, engines.values())
        engine->updateForum();
}

void Siilihai::updateClicked(ForumSubscription* sub , bool force) {
    qDebug() << Q_FUNC_INFO << "Update selected clicked, updating forum " << sub->toString()
            << ", force=" << force;
    Q_ASSERT(engines.contains(sub));
    engines[sub]->updateForum(force);
}

void Siilihai::cancelClicked() {
    foreach(ParserEngine* engine, engines.values())
        engine->cancelOperation();
}

void Siilihai::reportClicked(ForumSubscription* forum) {
    if (forum) {
        ForumParser parserToReport = pdb.getParser(forum->parser());
        ReportParser *rpt = new ReportParser(mainWin, forum->parser(),
                                             parserToReport.parser_name);
        connect(rpt, SIGNAL(parserReport(ParserReport)), &protocol,
                SLOT(sendParserReport(ParserReport)));
        rpt->exec();
    }
}

void Siilihai::statusChanged(ForumSubscription* forum, bool reloading, float progress) {
}

void Siilihai::showUnsubscribeForum(ForumSubscription* fs) {
    if (fs) {
        QMessageBox msgBox(mainWin);
        msgBox.setText("Really unsubscribe from forum?");
        msgBox.setInformativeText(fs->alias());
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgBox.setDefaultButton(QMessageBox::No);
        if (msgBox.exec() == QMessageBox::Yes) {
            protocol.subscribeForum(fs, true);
            fdb.deleteForum(fs);
            pdb.deleteParser(fs->parser());
        }
    }
}

void Siilihai::launchParserMaker() {
#ifndef Q_WS_HILDON
    if (!parserMaker) {
        parserMaker = new ParserMaker(mainWin, pdb, settings, protocol);
        connect(parserMaker, SIGNAL(destroyed()), this,
                SLOT(parserMakerClosed()));
        connect(parserMaker, SIGNAL(parserSaved(ForumParser)), this,
                SLOT(updateForumParser(ForumParser)));

    } else {
        parserMaker->showNormal();
    }
#endif
}

void Siilihai::parserMakerClosed() {
#ifndef Q_WS_HILDON
    if (parserMaker)
        disconnect(parserMaker);
#endif
    parserMaker->deleteLater();
    parserMaker = 0;
}

void Siilihai::sendParserReportFinished(bool success) {
    qDebug() << Q_FUNC_INFO << success;
    if (!success) {
        errorDialog("Sending report failed. Please check network connection.");
    } else {
        errorDialog("Thanks for your report");
    }
}

void Siilihai::subscribeForumFinished(bool success) {
    qDebug() << Q_FUNC_INFO << success;
    if (!success) {
        errorDialog("Subscribing to forum failed. Please check network connection.");
    }
}

void Siilihai::userSettingsReceived(bool success, UserSettings *newSettings) {
    qDebug() << Q_FUNC_INFO << success;
    if (!success) {
        errorDialog("Getting settings failed. Please check network connection.");
    } else {
        usettings = *newSettings;
        settings.setValue("preferences/sync_enabled", usettings.syncEnabled);
        settings.sync();
        settingsChanged(false);
    }
}

void Siilihai::settingsChanged(bool byUser) {
    usettings.syncEnabled = settings.value("preferences/sync_enabled", false).toBool();
    if(byUser) {
        protocol.setUserSettings(&usettings);
    }
}
