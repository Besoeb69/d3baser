#include "server.h"

Server::Server() {
    if (this->listen(QHostAddress::Any, tcpServerPort)) {
        initUdpSocket();
        timer = new QTimer(this);
        timer->start(TIME_FRAME);
        qDebug() << "Start...";
    }
    else {
        qDebug() << "Error";
    }

    connect(timer, &QTimer::timeout, this, [this](){
        if(BufferData.empty()){
            return;
        }

        QHash<quint32, QByteArray> mixedAudio;
        for (auto head = connectedTcpClients.begin(); head!=connectedTcpClients.end(); ++head){
            //отправлять надо только тем, кто подключился к udp каналу...ebal - maybe later (or never xD)
            quint32 recieverAddress = head.key()->peerAddress().toIPv4Address();
            opus_int32 mixedPcm32[FRAME_SIZE] {};

            for(auto head = BufferData.begin(); head!=BufferData.end(); ++head){
                quint32 senderAddress = head.key();
                if (recieverAddress!=senderAddress){
                    if(decoders.contains(head.key())){
                        const QByteArray data = head.value();
                        opus_int16 decodedPcm[FRAME_SIZE];
                        int frameSize = opus_decode(
                            decoders[head.key()],
                            reinterpret_cast<const unsigned char*>(data.constData()),
                            data.size(),
                            decodedPcm,
                            FRAME_SIZE,
                            0
                        );
                        if(frameSize < 0){
                            qDebug() << "Decode Error: " << opus_strerror(frameSize);
                            continue;
                        }

                        for (int i = 0; i<FRAME_SIZE; ++i){
                            mixedPcm32[i] += static_cast<opus_int32>(decodedPcm[i]);
                        }
                    }
                }
            }

            opus_int16 mixedPcm16[FRAME_SIZE] {};
            for(int i = 0; i<FRAME_SIZE;++i ){
                mixedPcm32[i] /= BufferData.size();

                if(mixedPcm32[i] >= 32767) {
                    mixedPcm16[i] = 32767;
                }
                else if (mixedPcm32[i] <= -32768) {
                    mixedPcm16[i] = -32768;
                }
                else {
                    mixedPcm16[i] = mixedPcm32[i];
                }
            }

            QByteArray pcmData(reinterpret_cast<const char*>(mixedPcm16), FRAME_SIZE * sizeof(opus_int16));
            mixedAudio.insert(recieverAddress, pcmData);
        }

        for(auto head = connectedTcpClients.begin(); head!=connectedTcpClients.end(); ++head){
            unsigned char opusPacket[4000];
            quint32 addr = head.key()->peerAddress().toIPv4Address();
            if(encoders.contains(addr)){
                const opus_int16* pcm = reinterpret_cast<const opus_int16*>(mixedAudio[addr].constData());
                opus_int32 len = opus_encode(encoders[addr], pcm, FRAME_SIZE, opusPacket, sizeof(opusPacket));
                //opus_int32 len = opus_encode(encoders[addr], mixedAudio[addr], FRAME_SIZE, opusPacket, sizeof(opusPacket));
                if (len <=0){
                    qDebug() << "Opus encode error:" << opus_strerror(len);
                    BufferData.clear();
                    return;
                }

                encodedData.insert(addr, QByteArray(reinterpret_cast<const char*>(opusPacket), len));
            }
        }

        if (!encodedData.isEmpty()) {
            for(auto it = connectedTcpClients.begin(); it != connectedTcpClients.end(); ++it){
                quint32 addr = it.key()->peerAddress().toIPv4Address();
                if(encodedData.contains(addr)){
                    for (int i = 0; i < encodedData.size(); i += MAX_UDP_SIZE) {
                        QByteArray chunk = encodedData[addr].mid(i, MAX_UDP_SIZE);
                        UdpSocket->writeDatagram(chunk, QHostAddress(addr), udpClientPort);
                    }
                }
            }
        }
        encodedData.clear();
        BufferData.clear();
    });
}

OpusEncoder* Server::initOpusEncoder(){
    int error;
    opus_int32 sampleRate = 48000;
    int channels = 1;
    int application = OPUS_APPLICATION_VOIP;

    OpusEncoder* encoder = opus_encoder_create(sampleRate, channels, application, &error);
    if (error != OPUS_OK){
        qDebug() << "Failed to create Opus encoder!";
    }

    opus_encoder_ctl(encoder, OPUS_SET_INBAND_FEC(1));
    opus_encoder_ctl(encoder, OPUS_SET_PACKET_LOSS_PERC(10));
    return encoder;
}

OpusDecoder* Server::initOpusDecoder(){
    int error;
    opus_int32 sampleRate = 48000;
    int channels = 1;

    OpusDecoder* decoder = opus_decoder_create(sampleRate, channels, &error);
    if (error != OPUS_OK){
        qDebug() << "Failed to create Opus decoder!";
    }
    return decoder;
}

void Server::incomingConnection(qintptr socketDescriptor) {
    QTcpSocket* socket = new QTcpSocket(this);
    socket->setSocketDescriptor(socketDescriptor);

    if (!connectedTcpClients.contains(socket)) {
        connectedTcpClients.insert(socket, "");
        connect(socket, &QTcpSocket::readyRead, this,&Server::slotReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &Server::slotClientDisconnected);
        quint32 addr = socket->peerAddress().toIPv4Address();
        encoders.insert(addr, initOpusEncoder());
        decoders.insert(addr, initOpusDecoder());
        //SendToClient("USER_CONN ", socket);
    }
    else {
        //SendToClient("Problem with connection", socket);
        qDebug() << socketDescriptor << " - Problem with accepting a socket";
    }
}

void Server::slotReadyRead() {
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    QString data = getSocketData(socket);
    qDebug() << data;

    if(connectedTcpClients.value(socket) == "") {
        QString nickname = data;
        if(nickname.startsWith("NICK_LOGIN ")) {
            nickname = nickname.mid(11);
            if(!nickname.isEmpty()){
                if(!connectedTcpClients.values().contains(nickname)){
                    connectedTcpClients[socket] = nickname;
                    SendToClient("NICK_ACCEPT", socket);
                    SendToAll("CHAT_MSG  * " + nickname + " is connected to server *");
                    qDebug() << "Client connected " << " - " << nickname;
                }
                else {
                    SendToClient("NICK_FALSE3", socket);
                    qDebug() << " - Nickname is already used";
                }
            }
            else {
                SendToClient("NICK_FALSE2", socket);
                qDebug() << " - Nickname is empty or forbidden";
            }
        }
        else {
            SendToClient("NICK_FALSE1", socket);
            qDebug() << "Problem with accepting a nickname - " << nickname;
        }
    }
    else if(data.startsWith("CHAT_MSG ")){
        data = data.mid(9);
        SendToAll("CHAT_MSG " + connectedTcpClients.value(socket) + ": " + data);
    }
}

void Server::slotClientDisconnected(){
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if(!socket) return;
    quint32 ip = socket->peerAddress().toIPv4Address();
    if (encoders.contains(ip)) {
        opus_encoder_destroy(encoders[ip]);
        encoders.remove(ip);
    }
    if (decoders.contains(ip)) {
        opus_decoder_destroy(decoders[ip]);
        decoders.remove(ip);
    }
    QString nick = connectedTcpClients[socket];
    if (connectedTcpClients.contains(socket)) {
        connectedTcpClients.remove(socket);
        SendToAll(" * " + nick + " is disconnected *");
        qDebug() << "Client disconnected:" << socket;
    }
    //disconnect(socket, &QTcpSocket::readyRead, this,&Server::slotReadyRead);
    //disconnect(socket, &QTcpSocket::disconnected, this, &Server::slotClientDisconnected);
    socket->deleteLater();
}

void Server::SendToClient(QString str, QTcpSocket* socket) {
    Data.clear();
    QDataStream out(&Data, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_2);
    out << quint16(0) << str;
    out.device()->seek(0);
    out << quint16(Data.size() - sizeof(quint16));
    socket->write(Data);
}

void Server::SendToAll(QString str) {
    Data.clear();
    QDataStream out(&Data, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_2);
    out << quint16(0) << str;
    out.device()->seek(0);
    out << quint16(Data.size() - sizeof(quint16));

    QList<QTcpSocket*> sockets = connectedTcpClients.keys();
    for (auto it = sockets.begin(); it != sockets.end(); ++it) {
        if (*it && (*it)->state() == QAbstractSocket::ConnectedState) {
            (*it)->write(Data);
        }
    }
}

QString Server::getSocketData(QTcpSocket* socket){
    QDataStream in(socket);
    quint16 nextBlockSize = 0;
    QString data;
    in.setVersion(QDataStream::Qt_6_2);
    if(in.status() == QDataStream::Ok){
        qDebug() << "Read tcp...";
        while(1) {
            if(nextBlockSize == 0) {
                if(socket->bytesAvailable() < 2) {
                    qDebug() << "Data < 2, break";
                    break;
                }
                in >> nextBlockSize;
            }
            if(socket->bytesAvailable() < nextBlockSize) {
                qDebug() << "Data not full, break";
                break;
            }
            in >> data;
            break;
        }
    }
    else {
        qDebug() << "DataStream error";
    }
    return data;
}

void Server::initUdpSocket() {
    UdpSocket = new QUdpSocket(this);
    UdpSocket->bind(QHostAddress::AnyIPv4, udpServerPort);
    connect(UdpSocket, &QUdpSocket::readyRead, this, &Server::readPendingDatagrams);
}

void Server::readPendingDatagrams() {
    while (UdpSocket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = UdpSocket->receiveDatagram();
        quint32 addr = datagram.senderAddress().toIPv4Address();
        QByteArray data = datagram.data();
        BufferData.insert(addr, data);
    }
}

void Server::stop() {
    close();
    for (auto it = connectedTcpClients.begin(); it != connectedTcpClients.end(); ++it) {
        it.key()->disconnectFromHost();
    }
}

Server::~Server(){
    close();
    for (auto it = encoders.begin(); it != encoders.end(); ++it) {
        opus_encoder_destroy(it.value());
    }
    for (auto it = decoders.begin(); it != decoders.end(); ++it) {
        opus_decoder_destroy(it.value());
    }
    encoders.clear();
    decoders.clear();
    for(auto it = connectedTcpClients.begin(); it != connectedTcpClients.end(); ++it) {
        if(it.key()) {
            it.key()->disconnectFromHost();
            it.key()->waitForDisconnected(1000);
            it.key()->deleteLater();
        }
    }
    connectedTcpClients.clear();
}
