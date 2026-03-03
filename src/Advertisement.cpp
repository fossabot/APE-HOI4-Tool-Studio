#include "Advertisement.h"
#include "LocalizationManager.h"
#include "ConfigManager.h"
#include <QPainter>
#include <QEvent>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QRandomGenerator>
#include <QDesktopServices>
#include <QUrl>
#include <QDebug>

Advertisement::Advertisement(QWidget *parent)
    : QWidget(parent)
    , m_countdownSeconds(5)
{
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
    QFile file(":/resources/advertisement/url.json");
    // Fallback to local file if resource not found
    if (!file.exists()) {
        file.setFileName("resources/advertisement/url.json");
    }
    
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray data = file.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isArray()) {
            QJsonArray array = doc.array();
            for (const QJsonValue &value : array) {
                if (value.isObject()) {
                    m_adList.append(value.toObject());
                }
            }
        }
        file.close();
    } else {
        qWarning() << "Failed to load advertisement data";
    }
}

bool Advertisement::selectRandomAd() {
    if (m_adList.isEmpty()) {
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
    
    // Load image
    QString imagePath = QString(":/resources/advertisement/%1.webp").arg(idStr);
    QPixmap pixmap(imagePath);
    
    // Fallback to local file
    if (pixmap.isNull()) {
        imagePath = QString("resources/advertisement/%1.webp").arg(idStr);
        pixmap.load(imagePath);
    }
    
    if (!pixmap.isNull()) {
        m_imageLabel->setPixmap(pixmap);
        return true;
    } else {
        qWarning() << "Failed to load advertisement image:" << imagePath;
        return false;
    }
}

void Advertisement::showAd() {
    if (!selectRandomAd()) {
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
    
    QString bgColor = isDark ? "rgba(30, 30, 30, 0.95)" : "rgba(255, 255, 255, 0.95)";
    QString containerBg = isDark ? "#2d2d2d" : "#ffffff";
    QString textColor = isDark ? "#ffffff" : "#333333";
    QString borderColor = isDark ? "#3d3d3d" : "#e0e0e0";
    QString btnBg = isDark ? "#007acc" : "#0078d7";
    QString btnBgHover = isDark ? "#0098ff" : "#1084d8";
    QString btnDisabledBg = isDark ? "#555555" : "#cccccc";
    
    setStyleSheet(QString(
        "Advertisement {"
        "   background-color: %1;"
        "}"
        "#adContainer {"
        "   background-color: %2;"
        "   border: 1px solid %3;"
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
        "   background-color: %7;"
        "   color: #888888;"
        "}"
    ).arg(bgColor, containerBg, borderColor, textColor, btnBg, btnBgHover, btnDisabledBg));
}

void Advertisement::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    painter.fillRect(rect(), palette().color(QPalette::Window));
    QWidget::paintEvent(event);
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
