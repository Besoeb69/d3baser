#ifndef DIALOG_LOGIN_H
#define DIALOG_LOGIN_H

#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui {
class DialogLogin;
}
QT_END_NAMESPACE

class LoginDialog : public QDialog {
    Q_OBJECT

public:
    LoginDialog(QDialog *parent = nullptr);
    ~ LoginDialog();

private:
    Ui::DialogLogin *ui;

signals:
    void get_pb_logindialog_ok();
    void get_le_nickname(const QString& nickname);
};

#endif // DIALOG_LOGIN_H
