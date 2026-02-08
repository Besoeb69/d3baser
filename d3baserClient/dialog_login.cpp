#include "dialog_login.h"
#include "./ui_dialog_login.h"

LoginDialog::LoginDialog(QDialog *parent) :
        QDialog(parent),
        ui(new Ui::DialogLogin)
{
    ui->setupUi(this);

    connect(ui->le_nickname, &QLineEdit::textChanged, this, [this](const QString& nick){
        emit get_le_nickname(nick);
    });

    connect(ui->pb_logindialog_ok, &QPushButton::clicked, this, [this]() {
        emit get_pb_logindialog_ok();
    });
};

LoginDialog::~LoginDialog(){
    delete ui;
}



