#ifndef THREADLISTWIDGET_H_
#define THREADLISTWIDGET_H_

#include <QTreeWidget>
#include <siilihai/forumgroup.h>
#include <siilihai/forumthread.h>
#include <siilihai/forummessage.h>
#include <siilihai/forumdatabase.h>

#include "messageformatting.h"

class ThreadListWidget : public QTreeWidget {
	Q_OBJECT

public:
	ThreadListWidget(QWidget *parent, ForumDatabase &f);
	virtual ~ThreadListWidget();
public slots:
	void groupSelected(ForumGroup *fg);
	void updateMessageRead(QTreeWidgetItem *item);
	void messageSelected(QTreeWidgetItem* item, QTreeWidgetItem *prev);
        void messageFound(ForumMessage *msg);
        void threadFound(ForumThread *thread);
        void messageUpdated(ForumMessage *msg);
        void messageDeleted(ForumMessage *msg);
        void threadDeleted(ForumThread *thread);
        void groupUpdated(ForumGroup *grp);
        void groupDeleted(ForumGroup *grp);
signals:
	void messageSelected(ForumMessage *msg);
private:
        QString messageSubject(ForumMessage *msg);
	void updateThreadUnreads(QTreeWidgetItem* threadItem);
        QTreeWidgetItem* messageWidget(ForumMessage *msg);
	QHash<QTreeWidgetItem*, ForumMessage*> forumMessages;
        QHash<QTreeWidgetItem*, QString> messageSubjects;
        ForumGroup *currentGroup;
	ForumDatabase &fdb;
};

#endif /* THREADLISTWIDGET_H_ */