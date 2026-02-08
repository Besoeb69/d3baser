#ifndef SERVER_H
#define SERVER_H

#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QNetworkDatagram>
#include <QTime>
#include <QTimer>

#include "opus/opus.h"

class Server : public QTcpServer {
    Q_OBJECT

public:
    Server();
    ~Server();
    void stop();

private:
    QHash<QTcpSocket*, QString> connectedTcpClients;
    QUdpSocket* UdpSocket;
    QByteArray Data;
    void SendToClient(QString str, QTcpSocket* socket);
    void SendToAll(QString str);
    QString getSocketData(QTcpSocket* socket);
    void initUdpSocket();
    void readPendingDatagrams();
    uint16_t tcpServerPort = 2323;
    uint16_t udpServerPort = 2424;
    uint16_t udpClientPort = 2525;

    QHash<quint32, OpusEncoder*> encoders;
    QHash<quint32, OpusDecoder*> decoders;
    QHash<quint32, QByteArray> BufferData;
    QHash<quint32, QByteArray> encodedData;
    QTimer *timer = nullptr;

    static const int TIME_FRAME = 20;
    static const int FRAME_SIZE = 960;
    static const int BYTES_PER_FRAME = FRAME_SIZE * sizeof(opus_int16);
    static const int MAX_UDP_SIZE = 508;

    OpusEncoder* initOpusEncoder();
    OpusDecoder* initOpusDecoder();

private slots:
    void incomingConnection(qintptr socketDescriptor);
    void slotReadyRead();
    void slotClientDisconnected();

};

#endif // SERVER_H
