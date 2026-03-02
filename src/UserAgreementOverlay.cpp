#include "UserAgreementOverlay.h"
#include "ConfigManager.h"
#include "LocalizationManager.h"
#include <QPainter>
#include <QPainterPath>
#include <QApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>

UserAgreementOverlay::UserAgreementOverlay(QWidget *parent)
    : QWidget(parent), m_isSettingsMode(false)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setAttribute(Qt::WA_TranslucentBackground);
    
    setupUi();
    updateTheme();
    updateTexts();
    
    hide();
    
    if (parent) {
        parent->installEventFilter(this);
    }
}

UserAgreementOverlay::~UserAgreementOverlay() {
}

void UserAgreementOverlay::setupUi() {
    m_container = new QWidget(this);
    m_container->setObjectName("UserAgreementContainer");
    m_container->setFixedSize(600, 450);
    
    QVBoxLayout *layout = new QVBoxLayout(m_container);
    layout->setContentsMargins(30, 25, 30, 25);
    layout->setSpacing(15);
    
    // Title
    m_titleLabel = new QLabel(m_container);
    m_titleLabel->setObjectName("UserAgreementTitle");
    m_titleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    layout->addWidget(m_titleLabel);
    
    // Text Browser for Markdown
    m_textBrowser = new QTextBrowser(m_container);
    m_textBrowser->setObjectName("UserAgreementText");
    m_textBrowser->setOpenExternalLinks(true);
    layout->addWidget(m_textBrowser);
    
    // Buttons
    m_buttonContainer = new QWidget(m_container);
    QHBoxLayout *btnLayout = new QHBoxLayout(m_buttonContainer);
    btnLayout->setContentsMargins(0, 0, 0, 0);
    btnLayout->setSpacing(15);
    
    m_rejectBtn = new QPushButton(m_buttonContainer);
    m_rejectBtn->setObjectName("RejectBtn");
    m_rejectBtn->setCursor(Qt::PointingHandCursor);
    m_rejectBtn->setFixedHeight(32);
    connect(m_rejectBtn, &QPushButton::clicked, this, &UserAgreementOverlay::onRejectClicked);
    
    m_acceptBtn = new QPushButton(m_buttonContainer);
    m_acceptBtn->setObjectName("AcceptBtn");
    m_acceptBtn->setCursor(Qt::PointingHandCursor);
    m_acceptBtn->setFixedHeight(32);
    connect(m_acceptBtn, &QPushButton::clicked, this, &UserAgreementOverlay::onAcceptClicked);
    
    btnLayout->addWidget(m_rejectBtn);
    btnLayout->addWidget(m_acceptBtn);
    
    layout->addWidget(m_buttonContainer);
}

void UserAgreementOverlay::updateTheme() {
    bool isDark = ConfigManager::instance().isCurrentThemeDark();
    
    QString containerBg = isDark ? "#2C2C2E" : "#FFFFFF";
    QString textColor = isDark ? "#FFFFFF" : "#1D1D1F";
    QString textBg = isDark ? "#1C1C1E" : "#F5F5F7";
    QString borderColor = isDark ? "#3A3A3C" : "#D2D2D7";
    QString primaryBtnBg = "#007AFF";
    QString primaryBtnHoverBg = "#0062CC";
    QString secondaryBtnBg = isDark ? "#3A3A3C" : "#E5E5EA";
    QString secondaryBtnHoverBg = isDark ? "#48484A" : "#D1D1D6";
    QString secondaryBtnText = isDark ? "#FFFFFF" : "#1D1D1F";
    
    m_container->setStyleSheet(QString(
        "QWidget#UserAgreementContainer {"
        "  background-color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 12px;"
        "}"
    ).arg(containerBg, borderColor));
    
    m_titleLabel->setStyleSheet(QString("color: %1;").arg(textColor));
    
    m_textBrowser->setStyleSheet(QString(
        "QTextBrowser#UserAgreementText {"
        "  background-color: %1;"
        "  color: %2;"
        "  border: 1px solid %3;"
        "  border-radius: 6px;"
        "  padding: 10px;"
        "}"
    ).arg(textBg, textColor, borderColor));
    
    m_acceptBtn->setStyleSheet(QString(
        "QPushButton#AcceptBtn {"
        "  background-color: %1;"
        "  color: #FFFFFF;"
        "  border: none;"
        "  border-radius: 6px;"
        "  font-weight: 500;"
        "}"
        "QPushButton#AcceptBtn:hover {"
        "  background-color: %2;"
        "}"
    ).arg(primaryBtnBg, primaryBtnHoverBg));
    
    m_rejectBtn->setStyleSheet(QString(
        "QPushButton#RejectBtn {"
        "  background-color: %1;"
        "  color: %2;"
        "  border: none;"
        "  border-radius: 6px;"
        "  font-weight: 500;"
        "}"
        "QPushButton#RejectBtn:hover {"
        "  background-color: %3;"
        "}"
    ).arg(secondaryBtnBg, secondaryBtnText, secondaryBtnHoverBg));
}

void UserAgreementOverlay::updateTexts() {
    LocalizationManager& loc = LocalizationManager::instance();
    
    QString titleTemplate = loc.getString("UserAgreement", "Title");
    if (titleTemplate.isEmpty()) {
        titleTemplate = "APE HOI4 Tool Studio User Agreement (%1)";
    }
    m_titleLabel->setText(titleTemplate.arg(m_currentUAV));
    
    if (m_isSettingsMode) {
        m_acceptBtn->setText(loc.getString("UserAgreement", "Close"));
        if (m_acceptBtn->text().isEmpty()) m_acceptBtn->setText("Close");
    } else {
        m_acceptBtn->setText(loc.getString("UserAgreement", "Accept"));
        if (m_acceptBtn->text().isEmpty()) m_acceptBtn->setText("Accept");
        
        m_rejectBtn->setText(loc.getString("UserAgreement", "Reject"));
        if (m_rejectBtn->text().isEmpty()) m_rejectBtn->setText("Reject");
    }
    
    loadAgreementContent();
}

QString UserAgreementOverlay::getUAVVersion() {
    QFile file(":/UserAgreement/version.json");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QByteArray data = file.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isObject()) {
            return doc.object()["UAV"].toString();
        }
    }
    return "0.0.0.0";
}

QString UserAgreementOverlay::getUAVCheckVersion() {
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/APE-HOI4-Tool-Studio";
    QFile file(tempDir + "/UAVCheck.json");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QByteArray data = file.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isObject()) {
            return doc.object()["UAVCheck"].toString();
        }
    }
    return "0.0.0.0";
}

void UserAgreementOverlay::saveUAVCheckVersion(const QString& version) {
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/APE-HOI4-Tool-Studio";
    QDir().mkpath(tempDir);
    
    QFile file(tempDir + "/UAVCheck.json");
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QJsonObject obj;
        obj["UAVCheck"] = version;
        QJsonDocument doc(obj);
        file.write(doc.toJson());
    }
}

void UserAgreementOverlay::loadAgreementContent() {
    QString lang = ConfigManager::instance().getLanguage();
    QString mdPath;
    
    if (lang == "简体中文") {
        mdPath = ":/UserAgreement/zh_CN.md";
    } else if (lang == "繁體中文") {
        mdPath = ":/UserAgreement/zh_TW.md";
    } else {
        mdPath = ":/UserAgreement/en_US.md";
    }
    
    QFile file(mdPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString content = QString::fromUtf8(file.readAll());
        m_textBrowser->setMarkdown(content);
    } else {
        m_textBrowser->setPlainText("Failed to load user agreement.");
    }
}

void UserAgreementOverlay::checkAgreement() {
    m_currentUAV = getUAVVersion();
    QString checkedUAV = getUAVCheckVersion();
    
    // Simple string comparison works for "YYYY.M.D.R" if zero-padded, 
    // but for safety, we should split and compare integers.
    // For simplicity, assuming standard format or simple string compare if length is same.
    // A robust version comparison:
    auto splitVersion = [](const QString& v) -> QList<int> {
        QList<int> parts;
        for (const QString& p : v.split('.')) {
            parts.append(p.toInt());
        }
        while (parts.size() < 4) parts.append(0);
        return parts;
    };
    
    QList<int> current = splitVersion(m_currentUAV);
    QList<int> checked = splitVersion(checkedUAV);
    
    bool needsUpdate = false;
    for (int i = 0; i < 4; ++i) {
        if (current[i] > checked[i]) {
            needsUpdate = true;
            break;
        } else if (current[i] < checked[i]) {
            break;
        }
    }
    
    if (needsUpdate) {
        showAgreement(false);
    } else {
        emit agreementAccepted();
    }
}

void UserAgreementOverlay::showAgreement(bool isSettingsMode) {
    m_isSettingsMode = isSettingsMode;
    m_currentUAV = getUAVVersion();
    
    m_rejectBtn->setVisible(!isSettingsMode);
    
    updateTexts();
    
    if (parentWidget()) {
        raise();
        updatePosition();
    }
    show();
}

void UserAgreementOverlay::onAcceptClicked() {
    if (!m_isSettingsMode) {
        saveUAVCheckVersion(m_currentUAV);
        emit agreementAccepted();
    }
    hide();
}

void UserAgreementOverlay::onRejectClicked() {
    if (!m_isSettingsMode) {
        emit agreementRejected();
        QApplication::quit();
    }
}

void UserAgreementOverlay::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QPainterPath path;
    QRectF r = rect();
    qreal radius = 10;
    
    path.addRoundedRect(r, radius, radius);
    
    painter.fillPath(path, QColor(0, 0, 0, 150));
}

bool UserAgreementOverlay::eventFilter(QObject *obj, QEvent *event) {
    if (obj == parentWidget() && event->type() == QEvent::Resize) {
        updatePosition();
    }
    return QWidget::eventFilter(obj, event);
}

void UserAgreementOverlay::updatePosition() {
    if (parentWidget()) {
        setGeometry(parentWidget()->rect());
        m_container->move(
            (width() - m_container->width()) / 2,
            (height() - m_container->height()) / 2
        );
    }
}
