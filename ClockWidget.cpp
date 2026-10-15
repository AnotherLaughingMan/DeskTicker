#include "ClockWidget.h"

#include <QAbstractAnimation>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QColorDialog>
#include <QEnterEvent>
#include <QContextMenuEvent>
#include <QCloseEvent>
#include <QDateTime>
#include <QDir>
#include <QDebug>
#include <QEasingCurve>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QFont>
#include <QFontMetrics>
#include <QFontMetricsF>
#include <QGraphicsOpacityEffect>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPropertyAnimation>
#include <QSizePolicy>
#include <QPen>
#include <QPushButton>
#include <QRect>
#include <QRectF>
#include <QScreen>
#include <QSaveFile>
#include <QSettings>
#include <QSlider>
#include <QStandardPaths>
#include <QXmlStreamWriter>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QVBoxLayout>

namespace {
constexpr int edgeSnapDistance = 24;

QRect windowPositionBounds(const QSize &windowSize, const QRect &available, int padding)
{
    const int horizontalPadding = qMin(padding, qMax(0, (available.width() - windowSize.width()) / 2));
    const int verticalPadding = qMin(padding, qMax(0, (available.height() - windowSize.height()) / 2));
    const int left = available.left() + horizontalPadding;
    const int top = available.top() + verticalPadding;
    const int right = qMax(left, available.right() - windowSize.width() + 1 - horizontalPadding);
    const int bottom = qMax(top, available.bottom() - windowSize.height() + 1 - verticalPadding);
    return QRect(QPoint(left, top), QPoint(right, bottom));
}

QPoint clampWindowPosition(const QPoint &position, const QSize &windowSize,
                           const QRect &available, int padding)
{
    const QRect bounds = windowPositionBounds(windowSize, available, padding);
    return {qBound(bounds.left(), position.x(), bounds.right()),
            qBound(bounds.top(), position.y(), bounds.bottom())};
}

QPoint magnetizeWindowPosition(const QPoint &position, const QSize &windowSize,
                               const QRect &available, int padding)
{
    const QRect bounds = windowPositionBounds(windowSize, available, padding);
    QPoint snapped = clampWindowPosition(position, windowSize, available, padding);

    if (qAbs(snapped.x() - bounds.left()) <= edgeSnapDistance)
        snapped.setX(bounds.left());
    else if (qAbs(snapped.x() - bounds.right()) <= edgeSnapDistance)
        snapped.setX(bounds.right());

    if (qAbs(snapped.y() - bounds.top()) <= edgeSnapDistance)
        snapped.setY(bounds.top());
    else if (qAbs(snapped.y() - bounds.bottom()) <= edgeSnapDistance)
        snapped.setY(bounds.bottom());

    return snapped;
}
}

ClockWidget::ClockWidget(QWidget *parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint)
{
    QSettings settings;
    m_alwaysOnTop = settings.value(QStringLiteral("alwaysOnTop"), false).toBool();
    m_startWithSystem = settings.value(QStringLiteral("startWithSystem"), false).toBool();
    m_backgroundOpacity = qBound(
        0, settings.value(QStringLiteral("backgroundOpacity"),
                          settings.value(QStringLiteral("opacity"), 75)).toInt(), 100);
    m_screenPadding = qBound(0, settings.value(QStringLiteral("screenPadding"), 12).toInt(), 100);
    m_widgetWidth = qBound(220, settings.value(QStringLiteral("widgetWidth"), 280).toInt(), 800);
    m_widgetHeight = qBound(90, settings.value(QStringLiteral("widgetHeight"), 112).toInt(), 600);
    m_timeFontSizePx = qBound(12, settings.value(QStringLiteral("timeFontSizePx"), 37).toInt(), 96);
    m_dateFontSizePx = qBound(8, settings.value(QStringLiteral("dateFontSizePx"), 13).toInt(), 48);
    m_boldText = settings.value(QStringLiteral("boldText"), false).toBool();
    m_backgroundColor = settings.value(QStringLiteral("backgroundColor"), m_backgroundColor).value<QColor>();
    m_textColor = settings.value(QStringLiteral("textColor"), m_textColor).value<QColor>();
    if (!m_backgroundColor.isValid())
        m_backgroundColor = QColor(24, 28, 36);
    if (!m_textColor.isValid())
        m_textColor = QColor(244, 246, 250);

#ifndef DESKTICKER_DEBUG_BUILD
    QString startupError;
    if (!setStartWithSystem(m_startWithSystem, &startupError))
        qWarning().noquote() << startupError;
#endif

    setWindowFlag(Qt::WindowStaysOnTopHint, m_alwaysOnTop);
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowTitle(tr("DeskTicker"));
    setMinimumSize(220, 90);
    resize(m_widgetWidth, m_widgetHeight);
    setMouseTracking(true);

    if (settings.contains(QStringLiteral("position"))) {
        const QPoint savedPosition = settings.value(QStringLiteral("position")).toPoint();
        QScreen *targetScreen = nullptr;
        const QRect savedGeometry(savedPosition, size());
        for (QScreen *screen : QGuiApplication::screens()) {
            if (screen->availableGeometry().intersects(savedGeometry)) {
                targetScreen = screen;
                break;
            }
        }
        if (!targetScreen)
            targetScreen = QGuiApplication::primaryScreen();

        if (targetScreen)
            move(clampWindowPosition(savedPosition, size(), targetScreen->availableGeometry(), m_screenPadding));
    }

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 12);
    layout->setSpacing(2);

    m_dragHandle = new QLabel(QStringLiteral("• • •"), this);
    m_dragHandle->setAlignment(Qt::AlignCenter);
    m_dragHandle->setFixedHeight(22);
    m_dragHandle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_dragHandle->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_dragHandle->setAutoFillBackground(true);
    QFont handleFont = m_dragHandle->font();
    handleFont.setPixelSize(10);
    handleFont.setWeight(QFont::Bold);
    m_dragHandle->setFont(handleFont);
    QPalette handlePalette = m_dragHandle->palette();
    handlePalette.setColor(QPalette::Window, m_backgroundColor);
    handlePalette.setColor(QPalette::WindowText, QColor(248, 247, 238));
    m_dragHandle->setPalette(handlePalette);

    auto *handleOpacity = new QGraphicsOpacityEffect(m_dragHandle);
    handleOpacity->setOpacity(0.0);
    m_dragHandle->setGraphicsEffect(handleOpacity);
    m_dragHandleAnimation = new QPropertyAnimation(handleOpacity, "opacity", this);
    m_dragHandleAnimation->setDuration(180);
    m_dragHandleAnimation->setStartValue(0.0);
    m_dragHandleAnimation->setEndValue(1.0);
    m_dragHandleAnimation->setEasingCurve(QEasingCurve::OutCubic);
    layout->addWidget(m_dragHandle);

    m_timeLabel = new QLabel(this);
    m_timeLabel->setContentsMargins(18, 0, 18, 0);
    m_timeLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_timeLabel->setAlignment(Qt::AlignCenter);
    QFont timeFont = m_timeLabel->font();
    timeFont.setWeight(QFont::Medium);
    m_timeLabel->setFont(timeFont);

    m_dateLabel = new QLabel(this);
    m_dateLabel->setContentsMargins(18, 0, 18, 0);
    m_dateLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_dateLabel->setAlignment(Qt::AlignCenter);

    layout->addWidget(m_timeLabel);
    layout->addWidget(m_dateLabel);

    applyAppearance();
    setupTrayIcon();

    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &ClockWidget::updateTime);
    timer->start(1000);
    updateTime();
}

void ClockWidget::updateTime()
{
    const QDateTime now = QDateTime::currentDateTime();
    m_timeText = QLocale().toString(now.time(), QLocale::ShortFormat);
    m_dateText = QLocale().toString(now.date(), QLocale::LongFormat);
    applyAppearance();
}

void ClockWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_isDragging = true;
        setCursor(Qt::ClosedHandCursor);
        m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void ClockWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isDragging && (event->buttons() & Qt::LeftButton)) {
        const QPoint globalPosition = event->globalPosition().toPoint();
        QScreen *screen = QGuiApplication::screenAt(globalPosition);
        if (!screen)
            screen = QGuiApplication::screenAt(frameGeometry().center());
        if (!screen)
            screen = QGuiApplication::primaryScreen();

        QPoint newPosition = globalPosition - m_dragOffset;
        if (screen)
            newPosition = magnetizeWindowPosition(newPosition, size(), screen->availableGeometry(),
                                                 m_screenPadding);
        move(newPosition);
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void ClockWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_isDragging = false;
        setCursor(underMouse() ? Qt::OpenHandCursor : Qt::ArrowCursor);
        saveSettings();
    }
    QWidget::mouseReleaseEvent(event);
}

void ClockWidget::enterEvent(QEnterEvent *event)
{
    QWidget::enterEvent(event);
    m_dragHandleAnimation->setDirection(QAbstractAnimation::Forward);
    m_dragHandleAnimation->start();
    if (!m_isDragging)
        setCursor(Qt::OpenHandCursor);
}

void ClockWidget::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
    m_dragHandleAnimation->setDirection(QAbstractAnimation::Backward);
    m_dragHandleAnimation->start();
    if (!m_isDragging)
        setCursor(Qt::ArrowCursor);
}

void ClockWidget::closeEvent(QCloseEvent *event)
{
    saveSettings();
    if (m_trayIcon)
        m_trayIcon->hide();
    event->accept();
    qApp->quit();
}

void ClockWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QColor backgroundColor = m_backgroundColor;
    backgroundColor.setAlphaF(m_backgroundOpacity / 100.0);
    const int borderAlpha = qRound(35.0 * m_backgroundOpacity / 100.0);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF bounds = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setPen(Qt::NoPen);
    painter.setBrush(backgroundColor);
    painter.drawRoundedRect(bounds, 12.0, 12.0);

    if (m_backgroundOpacity == 0) {
        QColor hitTargetColor = m_backgroundColor;
        hitTargetColor.setAlpha(1);
        const qreal radius = 11.0;
        const qreal bottom = qMin(bounds.bottom(), bounds.top() + 22.0);
        QPainterPath hitTargetPath;
        hitTargetPath.moveTo(bounds.left() + radius, bounds.top());
        hitTargetPath.lineTo(bounds.right() - radius, bounds.top());
        hitTargetPath.quadTo(bounds.right(), bounds.top(), bounds.right(), bounds.top() + radius);
        hitTargetPath.lineTo(bounds.right(), bottom);
        hitTargetPath.lineTo(bounds.left(), bottom);
        hitTargetPath.lineTo(bounds.left(), bounds.top() + radius);
        hitTargetPath.quadTo(bounds.left(), bounds.top(), bounds.left() + radius, bounds.top());
        hitTargetPath.closeSubpath();
        painter.fillPath(hitTargetPath, hitTargetColor);
    }

    painter.setPen(QPen(QColor(255, 255, 255, borderAlpha), 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(bounds, 12.0, 12.0);
}

void ClockWidget::showOptionsDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("DeskTicker Options"));
    dialog.setMinimumWidth(340);

    auto *dialogLayout = new QVBoxLayout(&dialog);
    auto *formLayout = new QFormLayout;

    auto *alwaysOnTopCheckBox = new QCheckBox(tr("Always on top"), &dialog);
    alwaysOnTopCheckBox->setChecked(m_alwaysOnTop);
    formLayout->addRow(alwaysOnTopCheckBox);

    auto *startWithSystemCheckBox = new QCheckBox(tr("Start with your system"), &dialog);
    startWithSystemCheckBox->setChecked(m_startWithSystem);
    formLayout->addRow(startWithSystemCheckBox);

    auto *boldTextCheckBox = new QCheckBox(tr("Bold text"), &dialog);
    boldTextCheckBox->setChecked(m_boldText);
    formLayout->addRow(boldTextCheckBox);

    const auto addPixelSlider = [&dialog, formLayout](const QString &labelText, int value,
                                                       int minimum, int maximum) {
        auto *row = new QWidget(&dialog);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        auto *slider = new QSlider(Qt::Horizontal, row);
        slider->setRange(minimum, maximum);
        slider->setValue(value);
        auto *valueLabel = new QLabel(QObject::tr("%1 px").arg(value), row);
        rowLayout->addWidget(slider);
        rowLayout->addWidget(valueLabel);
        QObject::connect(slider, &QSlider::valueChanged, valueLabel,
                         [valueLabel](int newValue) {
                             valueLabel->setText(QObject::tr("%1 px").arg(newValue));
                         });
        formLayout->addRow(labelText, row);
        return slider;
    };

    auto *widthSlider = addPixelSlider(tr("Widget width"), m_widgetWidth, 220, 800);
    auto *heightSlider = addPixelSlider(tr("Widget height"), m_widgetHeight, 90, 600);
    auto *timeFontSizeSlider = addPixelSlider(tr("Time text size"), m_timeFontSizePx, 12, 96);
    auto *dateFontSizeSlider = addPixelSlider(tr("Date text size"), m_dateFontSizePx, 8, 48);

    auto *opacitySlider = new QSlider(Qt::Horizontal, &dialog);
    opacitySlider->setRange(0, 100);
    opacitySlider->setValue(m_backgroundOpacity);
    auto *opacityValueLabel = new QLabel(tr("%1%").arg(m_backgroundOpacity), &dialog);
    auto *opacityRow = new QWidget(&dialog);
    auto *opacityLayout = new QHBoxLayout(opacityRow);
    opacityLayout->setContentsMargins(0, 0, 0, 0);
    opacityLayout->addWidget(opacitySlider);
    opacityLayout->addWidget(opacityValueLabel);
    connect(opacitySlider, &QSlider::valueChanged, &dialog,
            [this, opacityValueLabel](int value) {
                opacityValueLabel->setText(QObject::tr("%1%").arg(value));
                m_backgroundOpacity = value;
                update();
            });
    formLayout->addRow(tr("Background opacity"), opacityRow);

    auto *paddingSlider = new QSlider(Qt::Horizontal, &dialog);
    paddingSlider->setRange(0, 100);
    paddingSlider->setValue(m_screenPadding);
    paddingSlider->setToolTip(tr("Distance to keep from screen edges while dragging."));
    auto *paddingValueLabel = new QLabel(tr("%1 px").arg(m_screenPadding), &dialog);
    auto *paddingRow = new QWidget(&dialog);
    auto *paddingLayout = new QHBoxLayout(paddingRow);
    paddingLayout->setContentsMargins(0, 0, 0, 0);
    paddingLayout->addWidget(paddingSlider);
    paddingLayout->addWidget(paddingValueLabel);
    connect(paddingSlider, &QSlider::valueChanged, &dialog,
            [paddingValueLabel](int value) {
                paddingValueLabel->setText(QObject::tr("%1 px").arg(value));
            });
    formLayout->addRow(tr("Screen edge padding"), paddingRow);

    const int originalOpacity = m_backgroundOpacity;
    const int originalWidgetWidth = m_widgetWidth;
    const int originalWidgetHeight = m_widgetHeight;
    const int originalTimeFontSizePx = m_timeFontSizePx;
    const int originalDateFontSizePx = m_dateFontSizePx;
    const bool originalBoldText = m_boldText;
    const QPoint originalPosition = pos();
    const QSize originalWidgetSize = size();
    const QPoint originalCenter = QRect(originalPosition, originalWidgetSize).center();
    const QColor originalBackgroundColor = m_backgroundColor;
    const QColor originalTextColor = m_textColor;

    const auto updateWindowSizePreview = [this, widthSlider, heightSlider, paddingSlider,
                                          originalCenter] {
        m_widgetWidth = widthSlider->value();
        m_widgetHeight = heightSlider->value();
        resize(m_widgetWidth, m_widgetHeight);
        move(originalCenter - QPoint((width() - 1) / 2, (height() - 1) / 2));
        applyAppearance();

        QScreen *screen = QGuiApplication::screenAt(originalCenter);
        if (!screen)
            screen = QGuiApplication::primaryScreen();
        if (screen)
            move(clampWindowPosition(pos(), size(), screen->availableGeometry(),
                                     paddingSlider->value()));
    };
    connect(widthSlider, &QSlider::valueChanged, &dialog,
            [updateWindowSizePreview](int) { updateWindowSizePreview(); });
    connect(heightSlider, &QSlider::valueChanged, &dialog,
            [updateWindowSizePreview](int) { updateWindowSizePreview(); });
    connect(timeFontSizeSlider, &QSlider::valueChanged, &dialog, [this](int value) {
        m_timeFontSizePx = value;
        applyAppearance();
    });
    connect(dateFontSizeSlider, &QSlider::valueChanged, &dialog, [this](int value) {
        m_dateFontSizePx = value;
        applyAppearance();
    });
    connect(boldTextCheckBox, &QCheckBox::toggled, &dialog, [this](bool bold) {
        m_boldText = bold;
        applyAppearance();
    });

    const auto restorePreview = [this, originalOpacity, originalWidgetWidth, originalWidgetHeight,
                                 originalTimeFontSizePx, originalDateFontSizePx, originalBoldText,
                                 originalPosition, originalWidgetSize, originalBackgroundColor,
                                 originalTextColor] {
        m_backgroundOpacity = originalOpacity;
        m_widgetWidth = originalWidgetWidth;
        m_widgetHeight = originalWidgetHeight;
        m_timeFontSizePx = originalTimeFontSizePx;
        m_dateFontSizePx = originalDateFontSizePx;
        m_boldText = originalBoldText;
        m_backgroundColor = originalBackgroundColor;
        m_textColor = originalTextColor;
        resize(originalWidgetSize);
        move(originalPosition);
        applyAppearance();
    };

    QColor backgroundColor = m_backgroundColor;
    QColor textColor = m_textColor;
    const auto addColorPicker = [this, &dialog, formLayout](const QString &labelText,
                                                            const QString &dialogTitle,
                                                            QColor &selectedColor,
                                                            bool isBackgroundColor) {
        auto *row = new QWidget(&dialog);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);

        auto *preview = new QLabel(row);
        preview->setFixedSize(36, 24);
        const auto updatePreview = [preview](const QColor &color) {
            preview->setStyleSheet(
                QStringLiteral("background-color: %1; border: 1px solid palette(mid);")
                    .arg(color.name(QColor::HexRgb)));
            preview->setToolTip(color.name(QColor::HexRgb));
        };
        updatePreview(selectedColor);

        auto *chooseButton = new QPushButton(tr("Choose..."), row);
        connect(chooseButton, &QPushButton::clicked, &dialog,
                [this, &dialog, &selectedColor, updatePreview, dialogTitle, isBackgroundColor] {
                    const QColor initialColor = selectedColor;
                    QColorDialog colorDialog(initialColor, &dialog);
                    colorDialog.setWindowTitle(dialogTitle);
                    colorDialog.setOption(QColorDialog::DontUseNativeDialog);

                    const auto applyColor = [this, &selectedColor, updatePreview, isBackgroundColor]
                                            (const QColor &color) {
                        if (!color.isValid())
                            return;
                        selectedColor = color;
                        if (isBackgroundColor) {
                            m_backgroundColor = color;
                            applyAppearance();
                        } else {
                            m_textColor = color;
                            applyAppearance();
                        }
                        updatePreview(color);
                    };
                    connect(&colorDialog, &QColorDialog::currentColorChanged,
                            &colorDialog, applyColor);

                    if (colorDialog.exec() == QDialog::Accepted)
                        applyColor(colorDialog.selectedColor());
                    else
                        applyColor(initialColor);
                });

        rowLayout->addWidget(preview);
        rowLayout->addWidget(chooseButton);
        rowLayout->addStretch();
        formLayout->addRow(labelText, row);
    };
    addColorPicker(tr("Background color"), tr("Choose background color"), backgroundColor, true);
    addColorPicker(tr("Text color"), tr("Choose text color"), textColor, false);

    dialogLayout->addLayout(formLayout);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    dialogLayout->addWidget(buttons);
    dialog.adjustSize();

    QScreen *previewScreen = QGuiApplication::screenAt(frameGeometry().center());
    if (!previewScreen)
        previewScreen = QGuiApplication::primaryScreen();
    if (previewScreen) {
        const QRect available = previewScreen->availableGeometry();
        int x = frameGeometry().right() + 13;
        if (x + dialog.width() > available.right() + 1)
            x = frameGeometry().left() - dialog.width() - 13;
        const int maxX = qMax(available.left(), available.right() - dialog.width() + 1);
        const int maxY = qMax(available.top(), available.bottom() - dialog.height() + 1);
        dialog.move(qBound(available.left(), x, maxX),
                    qBound(available.top(), frameGeometry().top(), maxY));
    }

    if (dialog.exec() != QDialog::Accepted) {
        restorePreview();
        return;
    }

    const bool requestedStartup = startWithSystemCheckBox->isChecked();
    QString startupError;
    if (!setStartWithSystem(requestedStartup, &startupError)) {
        restorePreview();
        QMessageBox::warning(this, tr("Startup setting failed"), startupError);
        return;
    }

    m_startWithSystem = requestedStartup;
    m_alwaysOnTop = alwaysOnTopCheckBox->isChecked();
    m_backgroundOpacity = opacitySlider->value();
    m_screenPadding = paddingSlider->value();
    m_widgetWidth = widthSlider->value();
    m_widgetHeight = heightSlider->value();
    m_timeFontSizePx = timeFontSizeSlider->value();
    m_dateFontSizePx = dateFontSizeSlider->value();
    m_boldText = boldTextCheckBox->isChecked();
    m_backgroundColor = backgroundColor;
    m_textColor = textColor;
    setAlwaysOnTop(m_alwaysOnTop);
    QScreen *screen = QGuiApplication::screenAt(frameGeometry().center());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (screen)
        move(clampWindowPosition(pos(), size(), screen->availableGeometry(), m_screenPadding));
    applyAppearance();

    saveSettings();
}

void ClockWidget::showAboutDialog()
{
    const QString aboutText = tr(
        "<h3>DeskTicker</h3>"
        "Version %1<br>"
        "Author: AnotherLaughingMan<br><br>"
        "Libraries and tools: Qt %2 Widgets, C++17, CMake, and MSVC v143.<br><br>"
        "A human directed an AI to help code this app.")
                                  .arg(QCoreApplication::applicationVersion(),
                                       QString::fromLatin1(qVersion()));
    QMessageBox::about(this, tr("About DeskTicker"), aboutText);
}

void ClockWidget::applyAppearance()
{
    setWindowOpacity(1.0);

    QFont timeFont = m_timeLabel->font();
    timeFont.setWeight(m_boldText ? QFont::Bold : QFont::Medium);
    QFont dateFont = m_dateLabel->font();
    dateFont.setWeight(m_boldText ? QFont::Bold : QFont::Normal);

    const int availableWidth = qMax(1, m_timeLabel->contentsRect().width());
    const int availableHeight = qMax(1, height() - 38);
    const auto fitsAtScale = [this, &timeFont, &dateFont, availableWidth, availableHeight]
                             (qreal scale) {
        QFont scaledTimeFont = timeFont;
        scaledTimeFont.setPixelSize(qMax(6, qRound(m_timeFontSizePx * scale)));
        QFont scaledDateFont = dateFont;
        scaledDateFont.setPixelSize(qMax(6, qRound(m_dateFontSizePx * scale)));
        const QFontMetricsF timeMetrics(scaledTimeFont);
        const QFontMetricsF dateMetrics(scaledDateFont);
        return timeMetrics.horizontalAdvance(m_timeText) <= availableWidth
               && dateMetrics.horizontalAdvance(m_dateText) <= availableWidth
               && timeMetrics.height() + dateMetrics.height() <= availableHeight;
    };

    qreal scale = 1.0;
    if (!fitsAtScale(scale)) {
        qreal low = 0.0;
        qreal high = 1.0;
        for (int i = 0; i < 20; ++i) {
            const qreal middle = (low + high) / 2.0;
            if (fitsAtScale(middle))
                low = middle;
            else
                high = middle;
        }
        scale = low;
    }

    timeFont.setPixelSize(qMax(6, qRound(m_timeFontSizePx * scale)));
    dateFont.setPixelSize(qMax(6, qRound(m_dateFontSizePx * scale)));
    m_timeLabel->setFont(timeFont);
    m_dateLabel->setFont(dateFont);
    m_timeLabel->setText(QFontMetrics(timeFont).elidedText(m_timeText, Qt::ElideRight,
                                                           availableWidth));
    m_dateLabel->setText(QFontMetrics(dateFont).elidedText(m_dateText, Qt::ElideRight,
                                                           availableWidth));

    QPalette labelPalette = m_timeLabel->palette();
    labelPalette.setColor(QPalette::WindowText, m_textColor);
    m_timeLabel->setPalette(labelPalette);
    m_dateLabel->setPalette(labelPalette);

    QPalette handlePalette = m_dragHandle->palette();
    handlePalette.setColor(QPalette::Window, m_backgroundColor);
    handlePalette.setColor(QPalette::WindowText, QColor(248, 247, 238));
    m_dragHandle->setPalette(handlePalette);
    update();
}

bool ClockWidget::synchronizeStartupRegistration(QString *errorMessage)
{
    QSettings settings;
    const bool enabled = settings.value(QStringLiteral("startWithSystem"), false).toBool();
    return setStartWithSystem(enabled, errorMessage);
}

bool ClockWidget::setStartWithSystem(bool enabled, QString *errorMessage)
{
    const QString applicationPath = QCoreApplication::applicationFilePath();
#if defined(Q_OS_WIN)
    QSettings startupSettings(
        QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
        QSettings::NativeFormat);
    const QString entryName = QCoreApplication::applicationName();
    if (enabled) {
        const QString command = QStringLiteral("\"%1\"")
                                    .arg(QDir::toNativeSeparators(applicationPath));
        startupSettings.setValue(entryName, command);
    } else {
        startupSettings.remove(entryName);
    }
    startupSettings.sync();
    if (startupSettings.status() != QSettings::NoError) {
        if (errorMessage)
            *errorMessage = tr("Windows could not update the current user's startup entry.");
        return false;
    }
    return true;
#elif defined(Q_OS_MACOS)
    const QString agentsDirectory = QDir::homePath() + QStringLiteral("/Library/LaunchAgents");
    const QString entryPath = agentsDirectory + QStringLiteral("/com.deskticker.app.plist");
    if (!enabled) {
        if (QFile::exists(entryPath) && !QFile::remove(entryPath)) {
            if (errorMessage)
                *errorMessage = tr("Could not remove DeskTicker from Login Items.");
            return false;
        }
        return true;
    }
    if (!QDir().mkpath(agentsDirectory)) {
        if (errorMessage)
            *errorMessage = tr("Could not create the macOS Login Items folder.");
        return false;
    }

    QSaveFile file(entryPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = tr("Could not write the macOS Login Item: %1").arg(file.errorString());
        return false;
    }
    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    xml.writeDTD(QStringLiteral("<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
                                "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">"));
    xml.writeStartElement(QStringLiteral("plist"));
    xml.writeAttribute(QStringLiteral("version"), QStringLiteral("1.0"));
    xml.writeStartElement(QStringLiteral("dict"));
    xml.writeTextElement(QStringLiteral("key"), QStringLiteral("Label"));
    xml.writeTextElement(QStringLiteral("string"), QStringLiteral("com.deskticker.app"));
    xml.writeTextElement(QStringLiteral("key"), QStringLiteral("ProgramArguments"));
    xml.writeStartElement(QStringLiteral("array"));
    xml.writeTextElement(QStringLiteral("string"), applicationPath);
    xml.writeEndElement();
    xml.writeTextElement(QStringLiteral("key"), QStringLiteral("RunAtLoad"));
    xml.writeEmptyElement(QStringLiteral("true"));
    xml.writeEndElement();
    xml.writeEndElement();
    xml.writeEndDocument();
    if (xml.hasError() || !file.commit()) {
        if (errorMessage)
            *errorMessage = tr("Could not save the macOS Login Item.");
        return false;
    }
    return true;
#elif defined(Q_OS_LINUX)
    const QString autostartDirectory = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
                                       + QStringLiteral("/autostart");
    const QString entryPath = autostartDirectory + QStringLiteral("/DeskTicker.desktop");
    if (!enabled) {
        if (QFile::exists(entryPath) && !QFile::remove(entryPath)) {
            if (errorMessage)
                *errorMessage = tr("Could not remove DeskTicker from the desktop autostart folder.");
            return false;
        }
        return true;
    }
    if (!QDir().mkpath(autostartDirectory)) {
        if (errorMessage)
            *errorMessage = tr("Could not create the desktop autostart folder.");
        return false;
    }

    QString escapedPath = applicationPath;
    escapedPath.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    escapedPath.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    escapedPath.replace(QLatin1Char('$'), QStringLiteral("\\$"));
    escapedPath.replace(QLatin1Char('`'), QStringLiteral("\\`"));
    QSaveFile file(entryPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = tr("Could not write the desktop autostart entry: %1").arg(file.errorString());
        return false;
    }
    const QByteArray desktopEntry = QStringLiteral(
        "[Desktop Entry]\nType=Application\nName=DeskTicker\nExec=\"%1\"\n"
        "Terminal=false\nX-GNOME-Autostart-enabled=true\n")
                                        .arg(escapedPath)
                                        .toUtf8();
    if (file.write(desktopEntry) != desktopEntry.size() || !file.commit()) {
        if (errorMessage)
            *errorMessage = tr("Could not save the desktop autostart entry.");
        return false;
    }
    return true;
#else
    Q_UNUSED(enabled);
    Q_UNUSED(applicationPath);
    if (errorMessage)
        *errorMessage = tr("Starting with the system is not supported on this platform.");
    return false;
#endif
}

void ClockWidget::saveSettings() const
{
    QSettings settings;
    settings.setValue(QStringLiteral("position"), pos());
    settings.setValue(QStringLiteral("alwaysOnTop"), m_alwaysOnTop);
    settings.setValue(QStringLiteral("startWithSystem"), m_startWithSystem);
    settings.setValue(QStringLiteral("backgroundOpacity"), m_backgroundOpacity);
    settings.setValue(QStringLiteral("screenPadding"), m_screenPadding);
    settings.setValue(QStringLiteral("widgetWidth"), m_widgetWidth);
    settings.setValue(QStringLiteral("widgetHeight"), m_widgetHeight);
    settings.setValue(QStringLiteral("timeFontSizePx"), m_timeFontSizePx);
    settings.setValue(QStringLiteral("dateFontSizePx"), m_dateFontSizePx);
    settings.setValue(QStringLiteral("boldText"), m_boldText);
    settings.setValue(QStringLiteral("backgroundColor"), m_backgroundColor);
    settings.setValue(QStringLiteral("textColor"), m_textColor);
    settings.sync();
}

void ClockWidget::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    connect(menu.addAction(tr("Options...")), &QAction::triggered,
            this, &ClockWidget::showOptionsDialog);
    connect(menu.addAction(tr("About DeskTicker")), &QAction::triggered,
            this, &ClockWidget::showAboutDialog);
    menu.addSeparator();
    connect(menu.addAction(tr("Quit")), &QAction::triggered, this, &QWidget::close);
    menu.exec(event->globalPos());
}

void ClockWidget::setAlwaysOnTop(bool enabled)
{
    const bool current = windowFlags().testFlag(Qt::WindowStaysOnTopHint);
    m_alwaysOnTop = enabled;
    if (current == enabled)
        return;

    const bool wasVisible = isVisible();
    const bool wasActive = isActiveWindow();
    setWindowFlag(Qt::WindowStaysOnTopHint, enabled);
    if (!wasVisible)
        return;

    show();
    if (enabled)
        raise();
    if (wasActive)
        activateWindow();
}

void ClockWidget::setupTrayIcon()
{
    m_trayIcon = new QSystemTrayIcon(QIcon(QStringLiteral(":/DeskTicker.ico")), this);
    m_trayIcon->setToolTip(tr("DeskTicker"));

    auto *trayMenu = new QMenu(this);
    QAction *visibilityAction = trayMenu->addAction(QString());
    connect(trayMenu, &QMenu::aboutToShow, this, [this, visibilityAction] {
        visibilityAction->setText(isVisible() ? tr("Hide DeskTicker") : tr("Show DeskTicker"));
    });
    connect(visibilityAction, &QAction::triggered, this, [this] {
        if (isVisible())
            hide();
        else
            showWidget();
    });

    trayMenu->addSeparator();
    QAction *optionsAction = trayMenu->addAction(tr("Options..."));
    connect(optionsAction, &QAction::triggered, this, [this] {
        if (!isVisible())
            showWidget();
        showOptionsDialog();
    });
    QAction *aboutAction = trayMenu->addAction(tr("About DeskTicker"));
    connect(aboutAction, &QAction::triggered, this, &ClockWidget::showAboutDialog);
    trayMenu->addSeparator();
    connect(trayMenu->addAction(tr("Quit")), &QAction::triggered,
            this, &QWidget::close);

    m_trayIcon->setContextMenu(trayMenu);
    connect(m_trayIcon, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger) {
                    if (isVisible())
                        hide();
                    else
                        showWidget();
                }
            });
    m_trayIcon->show();
}

void ClockWidget::showWidget()
{
    show();
    raise();
    activateWindow();
}
