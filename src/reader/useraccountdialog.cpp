#include "useraccountdialog.h"
#include "ui_useraccountdialog.h"
#include <QMessageBox>
#include <QDebug>

UserAccountDialog::UserAccountDialog(QWidget *parent, QSettings *s) :
    QDialog(parent), ui(new Ui::UserAccountDialog), settings(s) {
    ui->setupUi(this);
    origUsername = settings->value("account/username", "").toString();
    origPassword = settings->value("account/password", "").toString();
    ui->userNameEdit->setText(origUsername);
    ui->passwordEdit->setText(origPassword);

    connect(ui->unregisterButton, SIGNAL(clicked()), this, SLOT(unregisterClicked()));
}

UserAccountDialog::~UserAccountDialog() {
    delete ui;
}

void UserAccountDialog::accept() {
    QDialog::accept();
    if(origUsername != ui->userNameEdit->text() || origPassword != ui->passwordEdit->text()) {
        // Changed
        settings->setValue("account/username", ui->userNameEdit->text());
        settings->setValue("account/password", ui->passwordEdit->text());
    }
}

void UserAccountDialog::unregisterClicked() {
    QMessageBox msgBox(this);
    msgBox.setText("Really unregister?");
    msgBox.setInformativeText("This will reset local data. Use with caution.");
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);
    if (msgBox.exec() == QMessageBox::Yes) {
        reject();
        emit unregisterSiilihai();
    }
}
