#ifndef THREADLISTWIDGET_H_
#define THREADLISTWIDGET_H_

#include <QTreeWidget>
#include <QList>
#include <QPair>
#include <QMenu>
#include <QContextMenuEvent>
#include <QObject>

#include <siilihai/forumgroup.h>
#include <siilihai/forumthread.h>
#include <siilihai/forummessage.h>
#include <siilihai/forumdatabase.h>

#include "messageformatting.h"
#include "threadlistmessageitem.h"
#include "threadlistthreaditem.h"
#include "threadlistshowmoreitem.h"

class ThreadListWidget : public QTreeWidget {
	Q_OBJECT

public:
	ThreadListWidget(QWidget *parent, ForumDatabase &f);
	virtual ~ThreadListWidget();

public slots:
	void groupSelected(ForumGroup *fg);
        void messageSelected(QTreeWidgetItem* item, QTreeWidgetItem *prev);
        void messageFound(ForumMessage *msg);
        void threadFound(ForumThread *thread);
        void groupChanged(ForumGroup *grp);
        void groupDeleted(QObject*);
        void markReadClicked(bool read=true);
        void markUnreadClicked();
        void threadPropertiesClicked();
        void viewInBrowserClicked();
        void selectNextUnread();
signals:
	void messageSelected(ForumMessage *msg);
        void moreMessagesRequested(ForumThread *thread);
        void viewInBrowser();
        void threadProperties(ForumThread *thread);
protected:
        void contextMenuEvent(QContextMenuEvent * event);

private:
        QString messageSubject(ForumMessage *msg);
        void swapMessages(ForumMessage *m1, ForumMessage *m2);
        void updateList();
        void clearList();
        void addThread(ForumThread *thread);
        void addMessage(ForumMessage *msg);
        void updateThreadUnreads(ThreadListThreadItem* threadItem);
        void addShowMoreButton(ForumThread *thread);
        // Helpers to quickly find correct items
        QHash<ThreadListThreadItem*, ForumThread*> forumThreads;
        QHash<ThreadListMessageItem*, ForumMessage*> forumMessages;
        ForumGroup *currentGroup;
	ForumDatabase &fdb;
        // Actions:
        QAction *markReadAction;
        QAction *markUnreadAction;
        QAction *threadPropertiesAction;
        QAction *viewInBrowserAction;
};

#endif /* THREADLISTWIDGET_H_ */
