#pragma once

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>

#include <functional>

class WebServer : public QObject {
    Q_OBJECT

public:
    explicit WebServer(QObject* parent = nullptr);
    ~WebServer() override;

    bool start(quint16 port = 8000);
    void stop();
    bool isRunning() const;
    void setPassword(const QString& password);

    void setInputHandlers(std::function<QStringList()> listFn,
                          std::function<QString()> currentFn,
                          std::function<bool(const QString&)> setFn);
    void setRecorderHandlers(std::function<void()> startFn,
                             std::function<void()> stopFn);
    void setStateHandlers(std::function<bool()> recordingFn,
                          std::function<QString()> statusFn,
                          std::function<QString()> fileFn,
                          std::function<QString()> formatFn,
                          std::function<qint64()> segmentStartFn);

public slots:
    void onVuLevels(float leftDb, float rightDb);
    void onAudioPcm16(const QByteArray& pcm16le, int sampleRate, int channels);

private slots:
    void onNewConnection();
    void onClientReadyRead();
    void onClientDisconnected();

private:
    void handleRequest(QTcpSocket* socket, const QString& path, const QString& query,
                       const QString& authHeader);
    void serveIndex(QTcpSocket* socket);
    void serveStatus(QTcpSocket* socket);
    void serveInputs(QTcpSocket* socket);
    void serveRec(QTcpSocket* socket);
    void serveStop(QTcpSocket* socket);
    void serveMonitor(QTcpSocket* socket, const QString& query);
    void serveAudioWav(QTcpSocket* socket);
    void serveSetInput(QTcpSocket* socket, const QString& query);

    void sendJson(QTcpSocket* socket, const QByteArray& json);
    void sendHtml(QTcpSocket* socket, const QByteArray& html);
    void sendError(QTcpSocket* socket, int code, const QByteArray& msg);

    QTcpServer* m_server{nullptr};
    QHash<QTcpSocket*, QByteArray> m_clientBuffers;

    float m_vuL{-60.0f};
    float m_vuR{-60.0f};
    QString m_password;
    bool m_webMonitorEnabled{false};
    QByteArray m_audioPcm16;
    int m_audioRate{48000};
    int m_audioChannels{2};

    std::function<QStringList()> m_listInputsFn;
    std::function<QString()> m_currentInputFn;
    std::function<bool(const QString&)> m_setInputFn;
    std::function<void()> m_startRecordingFn;
    std::function<void()> m_stopRecordingFn;
    std::function<bool()> m_isRecordingFn;
    std::function<QString()> m_statusFn;
    std::function<QString()> m_fileFn;
    std::function<QString()> m_formatFn;
    std::function<qint64()> m_segmentStartFn;
};