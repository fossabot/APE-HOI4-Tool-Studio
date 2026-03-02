#ifndef USERAGREEMENTOVERLAY_H
#define USERAGREEMENTOVERLAY_H

#include <QWidget>
#include <QLabel>
#include <QTextBrowser>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>

class UserAgreementOverlay : public QWidget {
    Q_OBJECT

public:
    explicit UserAgreementOverlay(QWidget *parent = nullptr);
    ~UserAgreementOverlay();

    void checkAgreement();
    void showAgreement(bool isSettingsMode = false);
    void updateTheme();
    void updateTexts();

signals:
    void agreementAccepted();
    void agreementRejected();

protected:
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onAcceptClicked();
    void onRejectClicked();

private:
    void updatePosition();
    void setupUi();
    void loadAgreementContent();
    QString getUAVVersion();
    QString getUAVCheckVersion();
    void saveUAVCheckVersion(const QString& version);

    QWidget *m_container;
    QLabel *m_titleLabel;
    QTextBrowser *m_textBrowser;
    QPushButton *m_acceptBtn;
    QPushButton *m_rejectBtn;
    QWidget *m_buttonContainer;

    QString m_currentUAV;
    bool m_isSettingsMode;
};

#endif // USERAGREEMENTOVERLAY_H
