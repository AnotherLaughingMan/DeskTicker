#pragma once

#include <QColor>
#include <QPoint>
#include <QString>
#include <QWidget>

class QCloseEvent;
class QContextMenuEvent;
class QEnterEvent;
class QLabel;
class QMouseEvent;
class QPaintEvent;
class QPropertyAnimation;
class QSystemTrayIcon;

class ClockWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit ClockWidget(QWidget *parent = nullptr);
    static bool synchronizeStartupRegistration(QString *errorMessage = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    void updateTime();
    void showOptionsDialog();
    void showAboutDialog();
    void applyAppearance();
    void saveSettings() const;
    static bool setStartWithSystem(bool enabled, QString *errorMessage = nullptr);
    void setAlwaysOnTop(bool enabled);
    void setupTrayIcon();
    void showWidget();

    QLabel *m_timeLabel = nullptr;
    QLabel *m_dateLabel = nullptr;
    QLabel *m_dragHandle = nullptr;
    QPropertyAnimation *m_dragHandleAnimation = nullptr;
    QSystemTrayIcon *m_trayIcon = nullptr;
    QPoint m_dragOffset;
    QString m_timeText;
    QString m_dateText;
    QColor m_backgroundColor{24, 28, 36};
    QColor m_textColor{244, 246, 250};
    int m_backgroundOpacity = 75;
    int m_screenPadding = 12;
    int m_widgetWidth = 280;
    int m_widgetHeight = 112;
    int m_timeFontSizePx = 37;
    int m_dateFontSizePx = 13;
    bool m_boldText = false;
    bool m_alwaysOnTop = false;
    bool m_startWithSystem = false;
    bool m_isDragging = false;
};
