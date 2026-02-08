#ifndef AUTHENTICATION_H
#define AUTHENTICATION_H

#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui {
class DialogConnect;
}
QT_END_NAMESPACE

class ConnectDialog : public QDialog {
    Q_OBJECT

public:
    ConnectDialog(QDialog *parent = nullptr);
    ~ConnectDialog();

private:
    Ui::DialogConnect *ui;

signals:
    void get_pb_connectdialog_ok();
    void get_le_ip(const QString& ip);
    void get_le_port(const uint16_t& port);
};

#endif // AUTHENTICATION_H
