#include "mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , dialog_connect(nullptr)
    , dialog_login(nullptr)
{
    ui->setupUi(this);
    TcpSocket = new QTcpSocket(this);

    connect(TcpSocket, &QTcpSocket::readyRead, this, &MainWindow::slotReadyRead);
    connect(this->ui->pushButton, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(&dialog_connect, &ConnectDialog::get_pb_connectdialog_ok, this, &MainWindow::slotConnectToHost);
    connect(&dialog_connect, &ConnectDialog::get_le_ip, this, &MainWindow::slot_le_ip);
    connect(&dialog_connect, &ConnectDialog::get_le_port, this, &MainWindow::slot_le_port);
    connect(TcpSocket, &QTcpSocket::stateChanged, this, [this](QTcpSocket::SocketState socketState){
        if (socketState == QAbstractSocket::UnconnectedState) {
            qDebug() << "UnconnectedState";
            this->ui->pushButton->setText("Connect to Server");
            ui->textBrowser->append("   <You are disconnected from Server>");//ne vsegda pravda => use code error
            if(UdpSocket){
                switchVoiceChat();
            }
        }
        else if(socketState == QAbstractSocket::ConnectingState) {
            qDebug() << "ConnectingState";
            this->ui->pushButton->setText("Connecting...");
        }
        else if(socketState == QAbstractSocket::ConnectedState){
            qDebug() << "ConnectedState";
            this->ui->pushButton->setText("Disconnect from Server");
            ui->textBrowser->append("   <You are connected to Server>");
        }
        else {
            return;
        }
    });

    //login
    connect(TcpSocket, &QTcpSocket::connected, this, [this]() {
        dialog_login.exec();
    });
    // connect(&dialog_login, &QDialog::rejected, this, [this]() {
    //     dialog_login.exec(); //сделать отдельную кнопку для ника и его смены
    // });
    connect(&dialog_login, &LoginDialog::get_le_nickname, this, &MainWindow::slot_le_nickname);
    connect(&dialog_login, &LoginDialog::get_pb_logindialog_ok, this, [this](){
        if(nickname != "") {
            SendToServer("NICK_LOGIN " + nickname);
            dialog_login.close();
        }
    });

    connect(TcpSocket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
        this, [this](QAbstractSocket::SocketError error) {
            qDebug() << "Connection error: " << TcpSocket->errorString();
            //add dialog window with error
    });

    //voice chat
    connect(this->ui->pbVoiceChannelConnect, &QPushButton::clicked, this, &MainWindow::switchVoiceChat);

    //audio transmission
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this](){ //deligate
        if (!UdpSocket) {
            return;
        }
        QByteArray newData = audioInputDevice->read(BYTES_PER_FRAME);
        audioBuffer.append(newData);

        while (audioBuffer.size() >= BYTES_PER_FRAME){
            QHostAddress target_ip(ip);
            QByteArray frame = audioBuffer.left(BYTES_PER_FRAME);
            audioBuffer = audioBuffer.mid(BYTES_PER_FRAME);

            const opus_int16* pcm = reinterpret_cast<const opus_int16*>(frame.constData());
            unsigned char opusPacket[4000];
            opus_int32 len = opus_encode(encoder, pcm, FRAME_SIZE, opusPacket, sizeof(opusPacket));

            if (len <=0){
                qDebug() << "Opus encode error:" << opus_strerror(len);
                continue;
            }
            qDebug() << "Encoded Opus size:" << len;
            //QString time = QTime::currentTime().toString("hh:mm:ss:zzz"); //for timestamp
            //QByteArray encodedData(time.toLatin1(), time.size());
            //encodedData.append(reinterpret_cast<const char*>(opusPacket), len);
            QByteArray encodedData(reinterpret_cast<const char*>(opusPacket), len);

            if (!encodedData.isEmpty()) {
                // for (int i = 0; i < encodedData.size(); i += MAX_UDP_SIZE) {
                //     QByteArray chunk = encodedData.mid(i, MAX_UDP_SIZE);
                //     UdpSocket->writeDatagram(chunk, target_ip, udpServerPort);
                // }
                UdpSocket->writeDatagram(encodedData, target_ip, udpServerPort);
            }
        }
    });
}

MainWindow::~MainWindow(){
    delete ui;
}

void MainWindow::onConnectClicked() {
    if (TcpSocket->state() == QAbstractSocket::UnconnectedState) {
        qDebug() << "UnconnectedState";
        dialog_connect.exec();
    }
    else if(TcpSocket->state() == QAbstractSocket::ConnectingState) {
        qDebug() << "ConnectingState";
        TcpSocket->disconnectFromHost();
    }
    else if(TcpSocket->state() == QAbstractSocket::ConnectedState){
        qDebug() << "ConnectedState";
        if (UdpSocket) {
            switchVoiceChat();
        }
        TcpSocket->disconnectFromHost();
    }
    else {
        qDebug() << "onConnectClicked error";
        return;
    }
}

void MainWindow::slotConnectToHost(){
    dialog_connect.close();
    if (TcpSocket->state() == QAbstractSocket::UnconnectedState) {
        TcpSocket->connectToHost(ip, tcpServerPort);
    }   
}

void MainWindow::slot_le_ip(const QString& slot_ip) {
    ip = slot_ip;
};

void MainWindow::slot_le_port(const uint16_t& slot_port){
    tcpServerPort = slot_port;
};

void MainWindow::slot_le_nickname(const QString& slot_nickname) {
    nickname = slot_nickname;
};

void MainWindow::SendToServer(QString str){
    Data.clear();
    QDataStream out(&Data, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_2);
    out << quint16(0) << str;
    out.device()->seek(0);
    out << quint16(Data.size() - sizeof(quint16));
    TcpSocket->write(Data);
    ui->lineEdit->clear();
}

void MainWindow::slotReadyRead(){
    quint16 nextBlockSize = 0;
    QDataStream in(TcpSocket);
    QString msg;
    in.setVersion(QDataStream::Qt_6_2);
    if (in.status()==QDataStream::Ok) {
        while(1) {
            if(nextBlockSize == 0) {
                if(TcpSocket->bytesAvailable() < 2) {
                    break;
                }
                in >> nextBlockSize;
            }
            if(TcpSocket->bytesAvailable() < nextBlockSize) {
                break;
            }
            in >>  msg;
            nextBlockSize = 0;
        }

        if(msg.startsWith("CHAT_MSG ")){ //chat message
            msg = msg.mid(9);
            QTime time = QTime::currentTime();
            ui->textBrowser->append(time.toString("hh:mm") + " " + msg);
        }
        else if(msg.startsWith("NICK_ACCEPT")){ //nick accepted
            //dialog_login.close();
            qDebug() << "Nick accepted";
        }
        else if(msg.startsWith("NICK_FALSE")){
            //dialog error with code 1/2/3... and then
            dialog_login.exec();
        }
    }
    else {
        ui->textBrowser->append("\tERROR - connection problem!");
    }
}

void MainWindow::on_pushButton_2_clicked(){
    QString msg = ui->lineEdit->text();
    if(msg != ""){
        SendToServer("CHAT_MSG " + msg);
    }
}

void MainWindow::on_lineEdit_returnPressed() {
    QString msg = ui->lineEdit->text();
    if(msg != ""){
        SendToServer("CHAT_MSG " + msg);
    }
}

// void MainWindow::initUdpSocket() { //useless??
//     if(UdpSocket){
//         UdpSocket->close();
//         disconnect(UdpSocket, &QUdpSocket::readyRead, this, &MainWindow::readPendingDatagrams);
//         delete UdpSocket;
//     }
//     UdpSocket = new QUdpSocket(this);
//     connect(UdpSocket, &QUdpSocket::readyRead, this, &MainWindow::readPendingDatagrams);
//     UdpSocket->bind(QHostAddress::AnyIPv4, udpClientPort);
//     UdpSocket->connectToHost(ip, udpServerPort);
// }

void MainWindow::initAudioOutput() {
    QAudioFormat format;
    format.setSampleRate(48000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);

    QAudioDevice info = QMediaDevices::defaultAudioOutput();
    if (!info.isFormatSupported(format)) {
        qDebug() << "Default output format not supported";
        return;
    }

    if (!output){
        output = new QAudioSink(format, this);
    }
    audioOutputDevice = output->start();
}

void MainWindow::startAudioTransmission(){
    QAudioFormat format;
    format.setSampleRate(48000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);

    QMediaDevices devices;

    QAudioDevice info = devices.defaultAudioInput();
    if (!info.isFormatSupported(format)) {
        format = info.preferredFormat();
        qDebug() << "Using preferred format instead.";
    }
    if(!input){
        input = new QAudioSource(format, this);
    }
    audioInputDevice = input->start();
    timer->start(TIME_FRAME);
}

void MainWindow::readPendingDatagrams() {

    while (UdpSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(UdpSocket->pendingDatagramSize());
        UdpSocket->readDatagram(datagram.data(), datagram.size());
        qDebug() << "UDP datagram size:" << datagram.size();
        opus_int16 decodedPcm[FRAME_SIZE];
        int frameSize = opus_decode(
            decoder,
            reinterpret_cast<const unsigned char*>(datagram.constData()),
            datagram.size(),
            decodedPcm,
            FRAME_SIZE,
            0
        );

        if(frameSize < 0){
            qDebug() << "Decode error:" << opus_strerror(frameSize);
            continue;
        }
        QByteArray audio(reinterpret_cast<const char*>(decodedPcm), frameSize * sizeof(opus_int16));
        if (audioOutputDevice) {
            audioOutputDevice->write(audio);
        }

        //else 1) numeration packages 2) bufferization
        //3)packet loss 4)owner of package?
    }
}

void MainWindow::initOpusEncoder(){
    int error;
    opus_int32 sampleRate = 48000;
    int channels = 1;
    int application = OPUS_APPLICATION_VOIP;

    encoder = opus_encoder_create(sampleRate, channels, application, &error);
    if (error != OPUS_OK){
        qDebug() << "Failed to create Opus encoder!";
    }

    opus_encoder_ctl(encoder, OPUS_SET_INBAND_FEC(1));
    opus_encoder_ctl(encoder, OPUS_SET_PACKET_LOSS_PERC(10));
}

void MainWindow::initOpusDecoder(){
    int error;
    opus_int32 sampleRate = 48000;
    int channels = 1;

    decoder = opus_decoder_create(sampleRate, channels, &error);
    if (error != OPUS_OK){
        qDebug() << "Failed to create Opus decoder!";
    }
}

void MainWindow::switchVoiceChat(){
    if (UdpSocket) {
        UdpSocket->close();
        UdpSocket->deleteLater();
        UdpSocket = nullptr;
        ui->pbVoiceChannelConnect->setText("Connect to Voice Channel");
        //stopAudioTransmission
        qDebug() << "OFF Voice Chat";
    } else if (TcpSocket->state() != QAbstractSocket::ConnectedState) {
        qDebug() << "TCP not connected";
        return;
    } else {
        initAudioOutput();
        initOpusEncoder();
        initOpusDecoder();

        //initUdpSocket();
        UdpSocket = new QUdpSocket(this);
        connect(UdpSocket, &QUdpSocket::readyRead, this, &MainWindow::readPendingDatagrams);
        UdpSocket->bind(QHostAddress::AnyIPv4, udpClientPort);
        UdpSocket->connectToHost(ip, udpServerPort);
        startAudioTransmission();
        ui->pbVoiceChannelConnect->setText("Disconnect from Voice");
        qDebug() << "ON Voice Chat - " << UdpSocket->state();
    }
}
















