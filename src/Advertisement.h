#ifndef ADVERTISEMENT_H
#define ADVERTISEMENT_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QTimer>
#include <QJsonObject>

class Advertisement : public QWidget {
    Q_OBJECT

public:
    explicit Advertisement(QWidget *parent = nullptr);
    ~Advertisement();

    void showAd();
    void hideAd();
    void updateTheme();

protected:
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void updateCountdown();
    void onImageClicked();

private:
    void updatePosition();
    void loadAdData();
    bool selectRandomAd();

    QWidget *m_container;
    QLabel *m_titleLabel;
    QLabel *m_imageLabel;
    QPushButton *m_closeButton;
    
    QTimer *m_countdownTimer;
    int m_countdownSeconds;
    
    QString m_currentUrl;
    QString m_currentText;
    
    QList<QJsonObject> m_adList;
};

#endif // ADVERTISEMENT_H
