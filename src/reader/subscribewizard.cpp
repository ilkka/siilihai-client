#include "subscribewizard.h"

SubscribeWizard::SubscribeWizard(QWidget *parent, SiilihaiProtocol &proto,
		QString &baseUrl) :
	QWizard(parent), protocol(proto) {
	selectedParser = 0;
	setWizardStyle(QWizard::ModernStyle);
	setPixmap(QWizard::WatermarkPixmap, QPixmap(
			":/data/siilis_wizard_watermark.png"));
	connect(this, SIGNAL(currentIdChanged(int)), this, SLOT(pageChanged(int)));
	addPage(createIntroPage());
	addPage(createLoginPage());
	addPage(createVerifyPage());
	setWindowTitle("Subscribe to a forum");
	connect(&protocol, SIGNAL(listParsersFinished(QList <ForumParser>)), this,
			SLOT(listParsersFinished(QList <ForumParser>)));
	connect(&protocol, SIGNAL(getParserFinished(ForumParser)), this,
			SLOT(getParserFinished(ForumParser)));

	connect(subscribeForm.searchString, SIGNAL(textEdited(QString)), this,
			SLOT(updateParserList()));
	connect(this, SIGNAL(accepted()), this, SLOT(wizardAccepted()));
	protocol.setBaseURL(baseUrl);
	progress = 0;
	show();
	protocol.listParsers();
	subscribeForm.forumList->addItem(QString(
			"Downloading list of available forums..."));
}

SubscribeWizard::~SubscribeWizard() {

}

QWizardPage *SubscribeWizard::createIntroPage() {
	QWizardPage *page = new QWizardPage;
	page->setTitle("Subscribe to a forum");
	page->setSubTitle("Please choose the forum you wish to subscribe to");

	QWidget *widget = new QWidget(this);
	QVBoxLayout *layout = new QVBoxLayout;
	subscribeForm.setupUi(widget);
	layout->addWidget(widget);
	page->setLayout(layout);
	return page;
}

void SubscribeWizard::listParsersFinished(QList<ForumParser> parsers) {
	allParsers = parsers;
	updateParserList();
}

void SubscribeWizard::updateParserList() {
	subscribeForm.forumList->clear();
	listWidgetItemForum.clear();
	for (int i = 0; i < allParsers.size(); i++) {
		if (allParsers[i].parser_name.contains(
				subscribeForm.searchString->text())
				|| allParsers[i].forum_url.contains(
						subscribeForm.searchString->text())) {
			// @todo type filtering
			QListWidgetItem *item =
					new QListWidgetItem(subscribeForm.forumList);
			item->setText(allParsers[i].parser_name);
			item->setToolTip(allParsers[i].forum_url);
			subscribeForm.forumList->addItem(item);
			listWidgetItemForum[item] = &allParsers[i];
		}
	}
}

QWizardPage *SubscribeWizard::createLoginPage() {
	QWizardPage *page = new QWizardPage;
	page->setTitle("Forum account");

	page->setSubTitle(
			"If you have registered to the forum, you can enter your account credentials here");
	QWidget *widget = new QWidget(this);
	QVBoxLayout *layout = new QVBoxLayout;
	subscribeForumLogin.setupUi(widget);
	layout->addWidget(widget);
	page->setLayout(layout);
	return page;
}

QWizardPage *SubscribeWizard::createVerifyPage() {
	QWizardPage *page = new QWizardPage;
	page->setTitle("Verify forum details");

	page->setSubTitle("Click Finish to subscribe to this forum");
	QWidget *widget = new QWidget(this);
	QVBoxLayout *layout = new QVBoxLayout;
	subscribeForumVerify.setupUi(widget);
	layout->addWidget(widget);
	page->setLayout(layout);
	return page;
}

void SubscribeWizard::pageChanged(int id) {
	if (id == 0) {
		selectedParser = 0;
	} else if (id == 1) {
		if (allParsers.size() == 0
				|| subscribeForm.forumList->selectedItems().size() != 1) {
			back();
		} else {
			selectedParser
					= listWidgetItemForum[subscribeForm.forumList->selectedItems()[0]];
			protocol.getParser(selectedParser->id);

			progress = new QProgressDialog("Downloading parser definition..",
					"Cancel", 0, 3, this);
			progress->setWindowModality(Qt::WindowModal);
			progress->setValue(0);
		}
	} else if (id == 2) {
		if (subscribeForumLogin.accountGroupBox->isChecked()) {

			progress = new QProgressDialog("Checking your credentials..",
					"Cancel", 0, 3, this);
			progress->setWindowModality(Qt::WindowModal);
			progress->setValue(0);
		}
		subscribeForumVerify.forumName->setText(selectedParser->parser_name);
		subscribeForumVerify.forumUrl->setText(selectedParser->forum_url);
		//		subscribeForumVerify.forumType->setText(selectedParser->parser_name);
	}
}

void SubscribeWizard::getParserFinished(ForumParser fp) {
	if (fp.id >= 0) {
		parser = fp;
	} else {
		QMessageBox msgBox;
		msgBox.setText(
				"Error: Unable to download parser definiton.\nCheck your network connection.");
		msgBox.exec();
		back();
	}
	if (progress) {
		progress->setValue(3);
		progress->deleteLater();
		progress = 0;
	}
}

void SubscribeWizard::wizardAccepted() {
	QString user = QString::null;
	QString pass = QString::null;

	if (subscribeForumLogin.accountGroupBox->isChecked()) {
		user = subscribeForumLogin.usernameEdit->text();
		pass = subscribeForumLogin.passwordEdit->text();
	}

	ForumSubscription fs;
	fs.parser = parser.id;
	fs.name = subscribeForumVerify.forumName->text();
	fs.username = user;
	fs.password = pass;
	fs.latest_threads = subscribeForumVerify.latestThreadsEdit->text().toInt();
	fs.latest_messages
			= subscribeForumVerify.latestMessagesEdit->text().toInt();

	emit(forumAdded(parser, fs));
}