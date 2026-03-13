#include "Update.h"
#include "ConfigManager.h"
#include "LocalizationManager.h"
#include "AuthManager.h"
#include "SslConfig.h"
#include <QPainter>
#include <QPainterPath>
#include <QApplication>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDir>
#include <QProcess>
#include <QDebug>
#include <QCryptographicHash>
#include <QTimer>

namespace {
QString formatBytesToMb(qint64 bytes) {
    double mb = bytes / 1024.0 / 1024.0;
    return QString::number(mb, 'f', 2);
}

QString formatDuration(qint64 seconds) {
    if (seconds < 0) {
        return QString("--:--");
    }
    qint64 hours = seconds / 3600;
    qint64 minutes = (seconds % 3600) / 60;
    qint64 secs = seconds % 60;
    if (hours > 0) {
        return QString("%1:%2:%3")
            .arg(hours, 2, 10, QChar('0'))
            .arg(minutes, 2, 10, QChar('0'))
            .arg(secs, 2, 10, QChar('0'));
    }
    return QString("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(secs, 2, 10, QChar('0'));
}
}

Update::Update(QWidget *parent)
    : QWidget(parent), m_networkManager(new QNetworkAccessManager(this)), m_downloadReply(nullptr), m_downloadFile(nullptr)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setAttribute(Qt::WA_TranslucentBackground);

    setupUi();
    updateTheme();

    hide();

    if (parent) {
        parent->installEventFilter(this);
    }
}

Update::~Update() {
    if (m_downloadReply) {
        m_downloadReply->abort();
        m_downloadReply->deleteLater();
    }
    if (m_downloadFile) {
        m_downloadFile->close();
        delete m_downloadFile;
    }
}

void Update::setupUi() {
    m_container = new QWidget(this);
    m_container->setObjectName("UpdateContainer");
    m_container->setFixedSize(400, 300);

    QVBoxLayout *layout = new QVBoxLayout(m_container);
    layout->setContentsMargins(30, 25, 30, 25);
    layout->setSpacing(15);
    
    // --- Checking Widget (matches LoadingOverlay) ---
    m_checkingWidget = new QWidget(m_container);
    QVBoxLayout *checkingLayout = new QVBoxLayout(m_checkingWidget);
    checkingLayout->setContentsMargins(0, 0, 0, 0);
    checkingLayout->setSpacing(15);
    checkingLayout->setAlignment(Qt::AlignCenter);
    
    // Icon
    m_iconLabel = new QLabel(m_checkingWidget);
    m_iconLabel->setPixmap(QPixmap(":/app.ico"));
    m_iconLabel->setAlignment(Qt::AlignCenter);
    checkingLayout->addWidget(m_iconLabel);
    
    // Message
    m_checkingMessageLabel = new QLabel(m_checkingWidget);
    m_checkingMessageLabel->setObjectName("UpdateCheckingMessage");
    m_checkingMessageLabel->setAlignment(Qt::AlignCenter);
    m_checkingMessageLabel->setWordWrap(true);
    checkingLayout->addWidget(m_checkingMessageLabel);
    
    // Progress bar
    m_checkingProgressBar = new QProgressBar(m_checkingWidget);
    m_checkingProgressBar->setObjectName("UpdateCheckingProgressBar");
    m_checkingProgressBar->setTextVisible(false);
    m_checkingProgressBar->setFixedHeight(6);
    m_checkingProgressBar->setRange(0, 0); // Indeterminate by default
    checkingLayout->addWidget(m_checkingProgressBar);
    
    layout->addWidget(m_checkingWidget);
    m_checkingWidget->hide();

    // --- Update Dialog Elements ---
    // Title
    m_titleLabel = new QLabel(m_container);
    m_titleLabel->setObjectName("UpdateTitle");
    m_titleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    layout->addWidget(m_titleLabel);

    // Version
    m_versionLabel = new QLabel(m_container);
    m_versionLabel->setObjectName("UpdateVersion");
    m_versionLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_versionLabel);

    // Changelog (QTextBrowser for Markdown rendering)
    m_changelogLabel = new QTextBrowser(m_container);
    m_changelogLabel->setObjectName("UpdateChangelog");
    m_changelogLabel->setReadOnly(true);
    m_changelogLabel->setOpenExternalLinks(true);
    m_changelogLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(m_changelogLabel);

    // Bottom Stack (Button vs Progress)
    m_bottomStack = new QStackedWidget(m_container);
    m_bottomStack->setFixedHeight(50); // Fixed height to prevent jumping

    // Page 0: Update Button
    QWidget *btnPage = new QWidget();
    QHBoxLayout *btnLayout = new QHBoxLayout(btnPage);
    btnLayout->setContentsMargins(0, 0, 0, 0);

    m_updateBtn = new QPushButton(btnPage);
    m_updateBtn->setObjectName("UpdateBtn");
    m_updateBtn->setCursor(Qt::PointingHandCursor);
    m_updateBtn->setFixedHeight(32);
    connect(m_updateBtn, &QPushButton::clicked, this, &Update::onUpdateClicked);
    btnLayout->addWidget(m_updateBtn);

    m_bottomStack->addWidget(btnPage);

    // Page 1: Progress Bar and Text
    QWidget *progressPage = new QWidget();
    QVBoxLayout *progressLayout = new QVBoxLayout(progressPage);
    progressLayout->setContentsMargins(0, 0, 0, 0);
    progressLayout->setSpacing(5);

    m_progressBar = new QProgressBar(progressPage);
    m_progressBar->setObjectName("UpdateProgressBar");
    m_progressBar->setTextVisible(false);
    m_progressBar->setFixedHeight(6);
    m_progressBar->setRange(0, 100);
    progressLayout->addWidget(m_progressBar);

    m_progressTextLabel = new QLabel(progressPage);
    m_progressTextLabel->setObjectName("UpdateProgressText");
    m_progressTextLabel->setAlignment(Qt::AlignCenter);
    progressLayout->addWidget(m_progressTextLabel);

    m_progressDetailLabel = new QLabel(progressPage);
    m_progressDetailLabel->setObjectName("UpdateProgressDetail");
    m_progressDetailLabel->setAlignment(Qt::AlignCenter);
    progressLayout->addWidget(m_progressDetailLabel);

    m_bottomStack->addWidget(progressPage);

    layout->addWidget(m_bottomStack);
}

void Update::updateTheme() {
    bool isDark = ConfigManager::instance().isCurrentThemeDark();

    QString containerBg = isDark ? "#2C2C2E" : "#FFFFFF";
    QString textColor = isDark ? "#FFFFFF" : "#1D1D1F";
    QString secondaryTextColor = isDark ? "#8E8E93" : "#86868B";
    QString borderColor = isDark ? "#3A3A3C" : "#D2D2D7";
    QString progressBg = isDark ? "#3A3A3C" : "#E5E5EA";
    QString progressChunk = "#007AFF";
    QString btnBg = isDark ? "#3A3A3C" : "#F2F2F7";
    QString btnHoverBg = isDark ? "#48484A" : "#E5E5EA";
    QString primaryBtnBg = "#007AFF";
    QString primaryBtnHoverBg = "#0062CC";

    m_container->setStyleSheet(QString(
        "QWidget#UpdateContainer {"
        "  background-color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 12px;"
        "}"
    ).arg(containerBg, borderColor));

    m_titleLabel->setStyleSheet(QString("color: %1;").arg(textColor));
    m_versionLabel->setStyleSheet(QString("color: %1; font-weight: 500;").arg(primaryBtnBg));
    m_changelogLabel->setStyleSheet(QString(
        "QTextBrowser#UpdateChangelog {"
        "  color: %1;"
        "  border: none;"
        "  background: transparent;"
        "}"
    ).arg(secondaryTextColor));
    m_progressTextLabel->setStyleSheet(QString("color: %1; font-size: 12px;").arg(secondaryTextColor));
    m_progressDetailLabel->setStyleSheet(QString("color: %1; font-size: 11px;").arg(secondaryTextColor));

    m_progressBar->setStyleSheet(QString(
        "QProgressBar#UpdateProgressBar {"
        "  background-color: %1;"
        "  border: none;"
        "  border-radius: 3px;"
        "}"
        "QProgressBar#UpdateProgressBar::chunk {"
        "  background-color: %2;"
        "  border-radius: 3px;"
        "}"
    ).arg(progressBg, progressChunk));
    
    m_checkingMessageLabel->setStyleSheet(QString(
        "QLabel#UpdateCheckingMessage {"
        "  color: %1;"
        "  font-size: 14px;"
        "  font-weight: 500;"
        "}"
    ).arg(textColor));
    
    m_checkingProgressBar->setStyleSheet(QString(
        "QProgressBar#UpdateCheckingProgressBar {"
        "  background-color: %1;"
        "  border: none;"
        "  border-radius: 3px;"
        "}"
        "QProgressBar#UpdateCheckingProgressBar::chunk {"
        "  background-color: %2;"
        "  border-radius: 3px;"
        "}"
    ).arg(progressBg, progressChunk));

    m_updateBtn->setStyleSheet(QString(
        "QPushButton#UpdateBtn {"
        "  background-color: %1;"
        "  color: #FFFFFF;"
        "  border: none;"
        "  border-radius: 6px;"
        "  font-weight: 500;"
        "}"
        "QPushButton#UpdateBtn:hover {"
        "  background-color: %2;"
        "}"
    ).arg(primaryBtnBg, primaryBtnHoverBg));

    // Update texts
    m_titleLabel->setText(LocalizationManager::instance().getString("Update", "new_version_title"));
    m_updateBtn->setText(LocalizationManager::instance().getString("Update", "update_now"));
}

void Update::checkForUpdates() {
    fetchManifest();
}

void Update::showCheckingOverlay() {
    m_isCheckingPhase = true;
    
    // Show checking state: hide update dialog elements, show checking widget
    m_titleLabel->hide();
    m_versionLabel->hide();
    m_changelogLabel->hide();
    m_bottomStack->hide();
    
    m_checkingWidget->show();
    m_checkingMessageLabel->setText(LocalizationManager::instance().getString("Update", "checking_for_updates"));
    
    // Resize container to match LoadingOverlay
    m_container->setFixedSize(350, 400);
    
    if (parentWidget()) {
        raise();
        updatePosition();
    }
    show();
}

void Update::fetchManifest() {
    QNetworkRequest request(QUrl(AuthManager::getApiBaseUrl() + "/api/v1/update/manifest"));
    request.setHeader(QNetworkRequest::UserAgentHeader, "APE-HOI4-Tool-Studio-Updater");
    SslConfig::applyPinnedConfiguration(request);

    QString token = AuthManager::instance().getToken();
    if (!token.isEmpty()) {
        request.setRawHeader("Authorization", QString("Bearer %1").arg(token).toUtf8());
    }

    // Also send the channel we are on
    QString channel = AuthManager::instance().getChannel();
    request.setRawHeader("X-Update-Channel", channel.toUtf8());

    // Send current version and language for cumulative changelog
    request.setRawHeader("X-Current-Version", QString(APP_VERSION).toUtf8());
    request.setRawHeader("X-Lang", LocalizationManager::instance().currentLang().toUtf8());

    QNetworkReply *reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::sslErrors, this, [reply](const QList<QSslError>& errors) {
        SslConfig::handleSslErrors(reply, errors);
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onManifestReceived(reply);
    });
}

void Update::onManifestReceived(QNetworkReply *reply) {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "Update check failed:" << reply->errorString();
        
        if (m_isCheckingPhase) {
            // Network error during checking phase - show retry message and auto-retry
            m_checkingMessageLabel->setText(LocalizationManager::instance().getString("Update", "check_failed_retrying"));
            
            if (!m_retryTimer) {
                m_retryTimer = new QTimer(this);
                m_retryTimer->setSingleShot(true);
                connect(m_retryTimer, &QTimer::timeout, this, &Update::fetchManifest);
            }
            m_retryTimer->start(5000); // Retry after 5 seconds
        }
        return;
    }

    QByteArray response = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(response);
    if (!doc.isObject()) {
        if (m_isCheckingPhase) {
            // Invalid response during checking phase - retry
            m_checkingMessageLabel->setText(LocalizationManager::instance().getString("Update", "check_failed_retrying"));
            if (!m_retryTimer) {
                m_retryTimer = new QTimer(this);
                m_retryTimer->setSingleShot(true);
                connect(m_retryTimer, &QTimer::timeout, this, &Update::fetchManifest);
            }
            m_retryTimer->start(5000);
        }
        return;
    }

    processManifest(doc.object());
}

void Update::processManifest(const QJsonObject &manifest) {
    QString latestVersion = manifest["version"].toString();
    // Use cumulative changelog from server (already localized and combined)
    QString changelog = manifest["combined_changelog"].toString();

    // Compare versions
    QString currentVersion = APP_VERSION;
    // Trigger update whenever version differs from server (upgrade or downgrade)
    if (latestVersion != currentVersion) {
        m_manifestFiles.clear();
        QJsonArray filesArray = manifest["files"].toArray();
        for (const QJsonValue &val : filesArray) {
            QJsonObject fileObj = val.toObject();
            UpdateFile uf;
            uf.path = fileObj["path"].toString();
            uf.hash = fileObj["hash"].toString();
            uf.url = fileObj["url"].toString();
            uf.size = fileObj["size"].toVariant().toLongLong();
            m_manifestFiles.append(uf);
        }

        bool wasChecking = m_isCheckingPhase;
        m_isCheckingPhase = false;
        
        showUpdateDialog(latestVersion, changelog);
        
        if (wasChecking) {
            emit updateCheckCompleted(true);
        }
    } else {
        // Already up to date
        if (m_isCheckingPhase) {
            m_isCheckingPhase = false;
            hide();
            emit updateCheckCompleted(false);
        }
    }
}

void Update::showUpdateDialog(const QString& version, const QString& changelog) {
    // Hide checking widget
    m_checkingWidget->hide();
    
    // Restore UI elements that may have been hidden during checking phase
    m_titleLabel->show();
    m_versionLabel->show();
    m_changelogLabel->show();
    m_bottomStack->show();
    m_bottomStack->setCurrentIndex(0); // Show update button
    m_progressBar->setRange(0, 100);   // Restore determinate mode
    m_container->setFixedSize(400, 300); // Restore full size
    
    m_versionLabel->setText(QString("v%1 -> v%2").arg(APP_VERSION, version));
    // Render Markdown content in QTextBrowser
    m_changelogLabel->setMarkdown(changelog);

    updateTheme();

    if (parentWidget()) {
        raise();
        updatePosition();
    }
    show();
}

void Update::onUpdateClicked() {
    m_bottomStack->setCurrentIndex(1);
    m_progressTextLabel->setText(LocalizationManager::instance().getString("Update", "starting_download"));
    m_progressDetailLabel->clear();

    startUpdateProcess();
}

QString Update::calculateFileHash(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (hash.addData(&file)) {
        return QString(hash.result().toHex());
    }
    return QString();
}

void Update::startUpdateProcess() {
    m_tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/APE-HOI4-Tool-Studio/update_cache";
    QDir().mkpath(m_tempDir);

    m_downloadQueue.clear();
    m_appDir = QCoreApplication::applicationDirPath();
    m_hasUpdaterFile = false;
    m_pendingUpdaterDownload = false;

    m_progressTextLabel->setText(LocalizationManager::instance().getString("Update", "checking_local_files"));
    m_progressDetailLabel->clear();
    QApplication::processEvents();

    for (const UpdateFile& uf : m_manifestFiles) {
        if (uf.path == "Updater.exe") {
            m_updaterFile = uf;
            m_hasUpdaterFile = true;
            continue;
        }

        QString localPath = QDir(m_appDir).filePath(uf.path);
        QString localHash = calculateFileHash(localPath);

        if (localHash != uf.hash) {
            m_downloadQueue.append(uf);
        }
    }

    m_pendingUpdaterDownload = m_hasUpdaterFile;
    m_totalFilesToDownload = m_downloadQueue.size() + (m_hasUpdaterFile ? 1 : 0);
    m_downloadedFilesCount = 0;
    m_totalBytesToDownload = 0;
    m_downloadedBytesTotal = 0;
    m_hasTotalSize = true;

    if (m_hasUpdaterFile) {
        if (m_updaterFile.size <= 0) {
            m_hasTotalSize = false;
        } else {
            m_totalBytesToDownload += m_updaterFile.size;
        }
    }

    for (const UpdateFile &uf : m_downloadQueue) {
        if (uf.size <= 0) {
            m_hasTotalSize = false;
            break;
        }
        m_totalBytesToDownload += uf.size;
    }

    if (m_totalFilesToDownload == 0) {
        m_progressTextLabel->setText(LocalizationManager::instance().getString("Update", "up_to_date"));
        QTimer::singleShot(1500, this, &Update::hide);
        return;
    }

    m_downloadTimer.invalidate();
    downloadNextFile();
}

void Update::downloadNextFile() {
    if (m_downloadQueue.isEmpty() && !m_pendingUpdaterDownload) {
        finishUpdate();
        return;
    }

    UpdateFile uf;
    QString targetPath;

    if (m_pendingUpdaterDownload) {
        uf = m_updaterFile;
        m_pendingUpdaterDownload = false;
        targetPath = QDir(m_appDir).filePath(uf.path);
    } else {
        uf = m_downloadQueue.takeFirst();
        targetPath = QDir(m_tempDir).filePath(uf.path);
    }

    QFileInfo fi(targetPath);
    QDir().mkpath(fi.absolutePath());

    m_currentFileName = fi.fileName();
    m_currentFileBytesTotal = uf.size;

    m_downloadFile = new QFile(targetPath);
    if (!m_downloadFile->open(QIODevice::WriteOnly)) {
        qDebug() << "Failed to open temp file for download:" << targetPath;
        downloadNextFile(); // Skip this file and continue
        return;
    }

    QString fullUrl = AuthManager::getApiBaseUrl() + uf.url;
    QNetworkRequest request((QUrl(fullUrl)));
    request.setHeader(QNetworkRequest::UserAgentHeader, "APE-HOI4-Tool-Studio-Updater");
    SslConfig::applyPinnedConfiguration(request);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    m_downloadReply = m_networkManager->get(request);

    connect(m_downloadReply, &QNetworkReply::sslErrors, this, [this](const QList<QSslError>& errors) {
        SslConfig::handleSslErrors(m_downloadReply, errors);
    });
    connect(m_downloadReply, &QNetworkReply::downloadProgress, this, &Update::onDownloadProgress);
    connect(m_downloadReply, &QNetworkReply::readyRead, this, &Update::onDownloadReadyRead);
    connect(m_downloadReply, &QNetworkReply::finished, this, &Update::onDownloadFinished);

    if (!m_downloadTimer.isValid()) {
        m_downloadTimer.start();
    }

    m_progressTextLabel->setText(LocalizationManager::instance().getString("Update", "downloading_file")
        .arg(m_currentFileName)
        .arg(m_downloadedFilesCount + 1)
        .arg(m_totalFilesToDownload));
}

void Update::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
    if (bytesTotal > 0) {
        m_currentFileBytesTotal = bytesTotal;
    }

    qint64 totalSoFar = m_downloadedBytesTotal + bytesReceived;
    if (m_hasTotalSize && m_totalBytesToDownload > 0) {
        int overallPercent = static_cast<int>(
            (static_cast<double>(totalSoFar) / static_cast<double>(m_totalBytesToDownload)) * 100.0
        );
        m_progressBar->setValue(overallPercent);
    } else if (bytesTotal > 0) {
        double fileWeight = 100.0 / m_totalFilesToDownload;
        double currentFileProgress = static_cast<double>(bytesReceived) / bytesTotal;
        int overallPercent = (m_downloadedFilesCount * fileWeight) + (currentFileProgress * fileWeight);
        m_progressBar->setValue(overallPercent);
    }

    double elapsedSeconds = m_downloadTimer.isValid() ? (m_downloadTimer.elapsed() / 1000.0) : 0.0;
    double speedBytes = (elapsedSeconds > 0.0) ? (totalSoFar / elapsedSeconds) : 0.0;
    double speedMb = speedBytes / 1024.0 / 1024.0;
    QString speedText = QString::number(speedMb, 'f', 2);

    if (m_hasTotalSize && m_totalBytesToDownload > 0) {
        qint64 remainingBytes = m_totalBytesToDownload - totalSoFar;
        qint64 etaSeconds = (speedBytes > 0.0) ? static_cast<qint64>(remainingBytes / speedBytes) : -1;
        m_progressDetailLabel->setText(LocalizationManager::instance().getString("Update", "download_detail_with_total")
            .arg(formatBytesToMb(totalSoFar))
            .arg(formatBytesToMb(m_totalBytesToDownload))
            .arg(speedText)
            .arg(formatDuration(etaSeconds)));
    } else {
        m_progressDetailLabel->setText(LocalizationManager::instance().getString("Update", "download_detail")
            .arg(formatBytesToMb(totalSoFar))
            .arg(speedText));
    }
}

void Update::onDownloadReadyRead() {
    if (m_downloadFile) {
        m_downloadFile->write(m_downloadReply->readAll());
    }
}

void Update::onDownloadFinished() {
    QString finishedPath;
    if (m_downloadFile) {
        finishedPath = m_downloadFile->fileName();
        m_downloadFile->close();
        delete m_downloadFile;
        m_downloadFile = nullptr;
    }

    if (m_downloadReply->error() == QNetworkReply::NoError) {
        m_downloadedFilesCount++;
        qint64 fileSize = m_currentFileBytesTotal;
        if (fileSize <= 0 && !finishedPath.isEmpty()) {
            fileSize = QFileInfo(finishedPath).size();
        }
        if (fileSize > 0) {
            m_downloadedBytesTotal += fileSize;
        }
    } else {
        qDebug() << "Download failed:" << m_downloadReply->errorString();
    }

    m_downloadReply->deleteLater();
    m_downloadReply = nullptr;

    downloadNextFile();
}

void Update::finishUpdate() {
    m_progressBar->setValue(100);
    m_progressTextLabel->setText(LocalizationManager::instance().getString("Update", "download_complete"));

    QString appDir = QCoreApplication::applicationDirPath();
    QString updaterPath = QDir(appDir).filePath("Updater.exe");

    if (!QFile::exists(updaterPath)) {
        qDebug() << "Updater.exe not found at:" << updaterPath;
        m_bottomStack->setCurrentIndex(0);
        m_updateBtn->setText("Updater missing - Retry");
        return;
    }

    // Write manifest file list so Updater can perform cleanup of obsolete files
    QString manifestListPath = QDir(m_tempDir).filePath("manifest_files.txt");
    QFile manifestListFile(manifestListPath);
    if (manifestListFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&manifestListFile);
        for (const UpdateFile& uf : m_manifestFiles) {
            stream << uf.path << "\n";
        }
        manifestListFile.close();
    } else {
        qDebug() << "Failed to write manifest_files.txt";
    }

    // Start Updater.exe with target dir and temp dir
    QStringList args;
    args << appDir << m_tempDir;
    
    QProcess::startDetached(updaterPath, args);

    // Quit application
    QApplication::quit();
}

void Update::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QPainterPath path;
    QRectF r = rect();
    qreal radius = 9;

    path.addRoundedRect(r, radius, radius);

    painter.fillPath(path, QColor(0, 0, 0, 120));
}

bool Update::eventFilter(QObject *obj, QEvent *event) {
    if (obj == parentWidget() && event->type() == QEvent::Resize) {
        updatePosition();
    }
    return QWidget::eventFilter(obj, event);
}

void Update::updatePosition() {
    if (parentWidget()) {
        setGeometry(parentWidget()->rect());
        m_container->move(
            (width() - m_container->width()) / 2,
            (height() - m_container->height()) / 2
        );
    }
}
