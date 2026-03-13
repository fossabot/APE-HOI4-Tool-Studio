#ifndef UPDATE_H
#define UPDATE_H

#include <QWidget>
#include <QLabel>
#include <QTextBrowser>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QPushButton>
#include <QStackedWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>
#include <QList>
#include <QJsonObject>
#include <QElapsedTimer>

struct UpdateFile {
    QString path;
    QString hash;
    QString url;
    qint64 size = -1;
};

class Update : public QWidget {
    Q_OBJECT

public:
    explicit Update(QWidget *parent = nullptr);
    ~Update();

    void checkForUpdates();
    void showCheckingOverlay();
    void updateTheme();

signals:
    void updateCheckCompleted(bool hasUpdate);

protected:
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadFinished();
    void onDownloadReadyRead();
    void onUpdateClicked();

private:
    void updatePosition();
    void setupUi();
    void showUpdateDialog(const QString& version, const QString& changelog);
    void startUpdateProcess();
    
    void fetchManifest();
    void onManifestReceived(QNetworkReply *reply);
    void processManifest(const QJsonObject &manifest);
    
    QString calculateFileHash(const QString& filePath);
    void downloadNextFile();
    void finishUpdate();

    QWidget *m_container;
    
    QWidget *m_checkingWidget;
    QLabel *m_iconLabel;
    QLabel *m_checkingMessageLabel;
    QProgressBar *m_checkingProgressBar;
    
    QLabel *m_titleLabel;
    QLabel *m_versionLabel;
    QTextBrowser *m_changelogLabel;
    QProgressBar *m_progressBar;
    QLabel *m_progressTextLabel;
    QLabel *m_progressDetailLabel;
    QPushButton *m_updateBtn;
    QStackedWidget *m_bottomStack;

    QNetworkAccessManager *m_networkManager;
    QNetworkReply *m_downloadReply;
    QFile *m_downloadFile;

    QString m_tempDir;
    QList<UpdateFile> m_manifestFiles;
    QList<UpdateFile> m_downloadQueue;

    QString m_appDir;
    UpdateFile m_updaterFile;
    bool m_hasUpdaterFile = false;
    bool m_pendingUpdaterDownload = false;
    
    int m_totalFilesToDownload;
    int m_downloadedFilesCount;

    qint64 m_totalBytesToDownload = 0;
    qint64 m_downloadedBytesTotal = 0;
    qint64 m_currentFileBytesTotal = 0;
    QString m_currentFileName;
    QElapsedTimer m_downloadTimer;
    bool m_hasTotalSize = false;
    
    bool m_isCheckingPhase = false;
    QTimer *m_retryTimer = nullptr;
};

#endif // UPDATE_H
