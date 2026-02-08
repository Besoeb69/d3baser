#include "authentication.h"
#include "./ui_authentication.h"

ConnectDialog::ConnectDialog(QDialog *parent) :
        QDialog(parent),
        ui(new Ui::DialogConnect)
{
    ui->setupUi(this);

    connect(ui->pb_connectdialog_ok, &QPushButton::clicked, this, [this](){
        emit get_pb_connectdialog_ok();
    });

    connect(ui->le_ip, &QLineEdit::textChanged, this, [this](const QString& ip){
        emit get_le_ip(ip);
    });

    connect(ui->le_port, &QLineEdit::textChanged, this, [this](const QString& port){
        emit get_le_port(port.toInt());
    });
};

ConnectDialog::~ConnectDialog(){
    delete ui;
};


