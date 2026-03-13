#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QTimer>
#include <QSplitter>
#include "SettingsPage.h"
#include "ConfigPage.h"
#include "ToolsPage.h"
#include "AccountPage.h"
#include "LoadingOverlay.h"
#include "Update.h"
#include "UserAgreementOverlay.h"
#include "Advertisement.h"
#include "LoginDialog.h"
#include "SetupDialog.h"
#include "ConnectionWarningOverlay.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onSettingsClicked();
    void onConfigClicked();
    void onToolsClicked();
    void onAccountClicked();
    void closeOverlay();
    void onToolSelected(const QString &toolId);
    
    // Actions from pages
    void onLanguageChanged();
    void onThemeChanged();
    void onDebugModeChanged(bool enabled);
    void onSidebarCompactChanged(bool enabled);
    void updateMemoryUsage();
    void onModClosed();
    void onGamePathChanged();
    void onModPathChanged();
    void onPathInvalid(const QString& titleKey, const QString& msgKey); // Updated signature
    void onToolProcessCrashed(const QString& toolId, const QString& error);
    void onLoginSuccessful();
    void onLogoutRequested();
    void onSetupCompleted();
    void onConnectionLost();
    void onConnectionRestored();

    // Window Controls
    void minimizeWindow();
    void maximizeWindow();
    void closeWindow();

    // Sidebar Animation
    void expandSidebar();
    void collapseSidebar();
    void toggleSidebarLock();
    
    // Advertisement
    void checkAndShowAdvertisement(bool loadFilesAfterAd = false);

private:
    void setupUi();
    void setupSidebar();
    void setupDebugOverlay();
    void loadStyle();
    void updateTexts();
    void applyTheme();
    
    // UI Components
    QWidget *m_centralWidget;
    QWidget *m_sidebar;
    QStackedWidget *m_mainStack; 
    QWidget *m_dashboard;
    QSplitter *m_dashboardSplitter;
    QWidget *m_dashboardContent;
    QWidget *m_rightSidebar;
    SettingsPage *m_settingsPage;
    ConfigPage *m_configPage;
    ToolsPage *m_toolsPage;
    AccountPage *m_accountPage;
    
    // Sidebar Widgets
    QWidget *m_windowControls; 
    QLabel *m_appIcon;
    QLabel *m_bottomAppIcon;
    QLabel *m_appTitle;
    QHBoxLayout *m_titleLayout; 
    QToolButton *m_toolsBtn;
    QToolButton *m_settingsBtn; 
    QToolButton *m_accountBtn;
    QToolButton *m_configBtn;   
    QVBoxLayout *m_sidebarLayout;
    QWidget *m_sidebarControlsContainer; 
    QWidget *m_controlsHorizontal; 
    QWidget *m_controlsVertical;   

    // Debug Overlay
    QLabel *m_memUsageLabel;
    QTimer *m_memTimer;
    
    // Loading Overlay
    LoadingOverlay *m_loadingOverlay;
    QTimer *m_scanCheckTimer;
    
    // Update Overlay
    Update *m_updateOverlay;
    
    // User Agreement Overlay
    UserAgreementOverlay *m_userAgreementOverlay;
    
    // Login Overlay
    LoginDialog *m_loginOverlay;
    
    // Advertisement Overlay
    Advertisement *m_advertisementOverlay;
    
    // Setup Overlay
    SetupDialog *m_setupOverlay;
    
    // Connection Warning Overlay
    ConnectionWarningOverlay *m_connectionWarningOverlay;
    
    // Startup sequence state
    bool m_startupSequenceStarted = false;
    bool m_setupSkipped = false;
    bool m_loginCompleted = false;
    bool m_pendingConnectionWarning = false;
    
    // Sidebar collapse delay timer
    QTimer *m_sidebarCollapseTimer;

    // Dragging state
    bool m_dragging = false;
    QPoint m_dragPosition;
    
    QString m_currentLang;
    bool m_sidebarExpanded = true;
    bool m_sidebarLocked = false;
    
    // First load tracking
    bool m_firstLoadCompleted = false;
};

#endif // MAINWINDOW_H
