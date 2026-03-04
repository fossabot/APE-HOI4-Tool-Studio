#include "Advertisement.h"
#include "LocalizationManager.h"
#include "ConfigManager.h"
#include "Logger.h"
#include <QPainter>
#include <QEvent>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QRandomGenerator>
#include <QDesktopServices>
#include <QUrl>
#include <QDebug>
#include <QPainterPath>

Advertisement::Advertisement(QWidget *parent)
    : QWidget(parent)
    , m_countdownSeconds(5)
{
    setAttribute(Qt::WA_TranslucentBackground);
    hide();
    
    // Create container
    m_container = new QWidget(this);
    m_container->setObjectName("adContainer");
    
    // Setup layout
    QVBoxLayout *layout = new QVBoxLayout(m_container);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(15);
    
    // Title
    m_titleLabel = new QLabel(m_container);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setObjectName("adTitle");
    
    // Image
    m_imageLabel = new QLabel(m_container);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setCursor(Qt::PointingHandCursor);
    m_imageLabel->setScaledContents(true);
    m_imageLabel->setMinimumSize(400, 300);
    m_imageLabel->installEventFilter(this);
    
    // Close Button
    m_closeButton = new QPushButton(m_container);
    m_closeButton->setObjectName("adCloseButton");
    m_closeButton->setMinimumHeight(40);
    m_closeButton->setCursor(Qt::PointingHandCursor);
    connect(m_closeButton, &QPushButton::clicked, this, &Advertisement::hideAd);
    
    layout->addWidget(m_titleLabel);
    layout->addWidget(m_imageLabel, 1);
    layout->addWidget(m_closeButton);
    
    // Timer
    m_countdownTimer = new QTimer(this);
    connect(m_countdownTimer, &QTimer::timeout, this, &Advertisement::updateCountdown);
    
    loadAdData();
    updateTheme();
}

Advertisement::~Advertisement() {
}

void Advertisement::loadAdData() {
    QString resPath = ":/advertisement/url.json";
    QString localPath = "resources/advertisement/url.json";
    
    QFile file(resPath);
    Logger::instance().logInfo("Advertisement", "Attempting to load ad data from: " + resPath);
    
    // Fallback to local file if resource not found
    if (!file.exists()) {
        Logger::instance().logInfo("Advertisement", "Resource not found, falling back to: " + localPath);
        file.setFileName(localPath);
    }
    
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray data = file.readAll();
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
        
        if (parseError.error != QJsonParseError::NoError) {
            Logger::instance().logError("Advertisement", "JSON parse error: " + parseError.errorString());
        } else if (doc.isArray()) {
            QJsonArray array = doc.array();
            for (const QJsonValue &value : array) {
                if (value.isObject()) {
                    m_adList.append(value.toObject());
                }
            }
            Logger::instance().logInfo("Advertisement", QString("Successfully loaded %1 ads").arg(m_adList.size()));
        } else {
            Logger::instance().logError("Advertisement", "JSON document is not an array");
        }
        file.close();
    } else {
        Logger::instance().logError("Advertisement", "Failed to open advertisement data file: " + file.errorString());
    }
}

bool Advertisement::selectRandomAd() {
    if (m_adList.isEmpty()) {
        Logger::instance().logError("Advertisement", "Cannot select random ad: ad list is empty");
        return false;
    }
    
    int index = QRandomGenerator::global()->bounded(m_adList.size());
    QJsonObject ad = m_adList.at(index);
    
    // Handle ID which might be a number or string
    QString idStr;
    if (ad.contains("id")) {
        if (ad["id"].isDouble()) {
            // Format as 8 digit string with leading zeros
            idStr = QString("%1").arg(ad["id"].toInt(), 8, 10, QChar('0'));
        } else {
            idStr = ad["id"].toString();
        }
    }
    
    m_currentText = ad["text"].toString();
    m_currentUrl = ad["url"].toString();
    
    Logger::instance().logInfo("Advertisement", QString("Selected ad ID: %1, Text: %2").arg(idStr, m_currentText));
    
    // Load image
    QString resPath = QString(":/advertisement/%1.png").arg(idStr);
    QString localPath = QString("resources/advertisement/%1.png").arg(idStr);
    
    QPixmap pixmap;
    bool loaded = pixmap.load(resPath);
    
    if (loaded) {
        Logger::instance().logInfo("Advertisement", "Loaded image from resource: " + resPath);
    } else {
        Logger::instance().logInfo("Advertisement", "Failed to load from resource, trying local: " + localPath);
        loaded = pixmap.load(localPath);
        if (loaded) {
            Logger::instance().logInfo("Advertisement", "Loaded image from local file: " + localPath);
        }
    }
    
    if (loaded && !pixmap.isNull()) {
        m_imageLabel->setPixmap(pixmap);
        return true;
    } else {
        Logger::instance().logError("Advertisement", "Failed to load advertisement image for ID: " + idStr);
        return false;
    }
}

void Advertisement::showAd() {
    Logger::instance().logInfo("Advertisement", "showAd() called");
    
    if (!selectRandomAd()) {
        Logger::instance().logError("Advertisement", "Aborting showAd() because selectRandomAd() failed");
        return; // Don't show if no ad could be loaded
    }
    
    // Update title with localized text
    QString titleFormat = LocalizationManager::instance().getString("Advertisement", "Advertisement.Title");
    if (titleFormat.isEmpty() || titleFormat == "Advertisement.Title") {
        titleFormat = "Advertisement - %1"; // Fallback
    }
    m_titleLabel->setText(titleFormat.arg(m_currentText));
    
    // Reset countdown
    m_countdownSeconds = 5;
    m_closeButton->setText(QString::number(m_countdownSeconds));
    m_closeButton->setEnabled(false);
    
    if (parentWidget()) {
        resize(parentWidget()->size());
    }
    
    updatePosition();
    show();
    raise();
    
    m_countdownTimer->start(1000);
}

void Advertisement::hideAd() {
    m_countdownTimer->stop();
    hide();
}

void Advertisement::updateCountdown() {
    m_countdownSeconds--;
    if (m_countdownSeconds > 0) {
        m_closeButton->setText(QString::number(m_countdownSeconds));
    } else {
        m_countdownTimer->stop();
        m_closeButton->setText(LocalizationManager::instance().getString("Common", "Close"));
        m_closeButton->setEnabled(true);
    }
}

void Advertisement::onImageClicked() {
    if (!m_currentUrl.isEmpty()) {
        QDesktopServices::openUrl(QUrl(m_currentUrl));
    }
}

void Advertisement::updateTheme() {
    bool isDark = ConfigManager::instance().isCurrentThemeDark();
    
    QString containerBg = isDark ? "#2d2d2d" : "#ffffff";
    QString textColor = isDark ? "#ffffff" : "#333333";
    QString borderColor = isDark ? "#3d3d3d" : "#e0e0e0";
    QString btnBg = isDark ? "#007acc" : "#0078d7";
    QString btnBgHover = isDark ? "#0098ff" : "#1084d8";
    QString btnDisabledBg = isDark ? "#555555" : "#cccccc";
    
    setStyleSheet(QString(
        "#adContainer {"
        "   background-color: %1;"
        "   border: 1px solid %2;"
        "   border-radius: 10px;"
        "}"
        "#adTitle {"
        "   color: %4;"
        "   font-size: 18px;"
        "   font-weight: bold;"
        "}"
        "#adCloseButton {"
        "   background-color: %5;"
        "   color: white;"
        "   border: none;"
        "   border-radius: 5px;"
        "   font-size: 14px;"
        "   font-weight: bold;"
        "}"
        "#adCloseButton:hover:!disabled {"
        "   background-color: %6;"
        "}"
        "#adCloseButton:disabled {"
        "   background-color: %6;"
        "   color: #888888;"
        "}"
    ).arg(containerBg, borderColor, textColor, btnBg, btnBgHover, btnDisabledBg));
}

void Advertisement::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Semi-transparent background with rounded corners on all sides to match main window
    QPainterPath path;
    QRectF r = rect();
    qreal radius = 10;
    
    path.addRoundedRect(r, radius, radius);
    
    painter.fillPath(path, QColor(0, 0, 0, 120));
}

bool Advertisement::eventFilter(QObject *obj, QEvent *event) {
    if (obj == m_imageLabel && event->type() == QEvent::MouseButtonRelease) {
        onImageClicked();
        return true;
    }
    
    if (event->type() == QEvent::Resize && parentWidget()) {
        resize(parentWidget()->size());
        updatePosition();
    }
    
    return QWidget::eventFilter(obj, event);
}

void Advertisement::updatePosition() {
    if (m_container) {
        // Center the container
        int x = (width() - m_container->width()) / 2;
        int y = (height() - m_container->height()) / 2;
        m_container->move(x, y);
    }
}
