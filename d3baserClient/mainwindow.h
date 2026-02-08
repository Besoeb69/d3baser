#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QNetworkDatagram>
#include <QAudioDevice>
#include <QMediaDevices>
#include <QAudioSource>
#include <QAudioSink>
#include <QTimer>
#include <QTime>

#include "./ui_mainwindow.h"
#include "authentication.h"
#include "dialog_login.h"
#include "opus/opus.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onConnectClicked();
    void on_pushButton_2_clicked();
    void on_lineEdit_returnPressed();
    void slotConnectToHost();
    void slot_le_nickname(const QString& nickname);
    void slot_le_ip(const QString& ip);
    void slot_le_port(const uint16_t& port);

    void slotReadyRead();
    //void initUdpSocket();
    void readPendingDatagrams();
    void startAudioTransmission();

private:
    Ui::MainWindow *ui = nullptr;
    QTcpSocket *TcpSocket = nullptr;
    QUdpSocket *UdpSocket = nullptr;
    QByteArray Data;
    void SendToServer(QString str);

    ConnectDialog dialog_connect;
    LoginDialog dialog_login;
    QString ip;
    uint16_t tcpServerPort = 2323;
    uint16_t udpServerPort = 2424;
    uint16_t udpClientPort = 2525;
    QString nickname = "";

    QAudioSource *input = nullptr;
    QAudioSink *output = nullptr;
    QIODevice *audioInputDevice = nullptr;
    QIODevice *audioOutputDevice = nullptr;
    OpusEncoder *encoder = nullptr;
    OpusDecoder *decoder = nullptr;
    QByteArray audioBuffer;
    QTimer *timer = nullptr;

    static const int TIME_FRAME = 20;
    static const int FRAME_SIZE = 960;
    static const int BYTES_PER_FRAME = FRAME_SIZE * sizeof(opus_int16);
    static const int MAX_UDP_SIZE = 1400;

    void switchVoiceChat();
    void initAudioOutput();
    void initOpusEncoder();
    void initOpusDecoder();

};
#endif // MAINWINDOW_H
