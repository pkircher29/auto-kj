/*
 * Copyright (c) 2025 Auto-KJ Team
 *
 * This file is part of Auto-KJ.
 *
 * Auto-KJ is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "tippanel.h"

#include <QHBoxLayout>
#include <QLocale>
#include <QDebug>

// ── TipToast ─────────────────────────────────────────────────────────────────

TipToast::TipToast(const QString &singerName, int amountCents,
                   const QString &message, const QString &currency,
                   QWidget *parent)
    : QFrame(parent)
{
    setObjectName("TipToast");
    setStyleSheet(
        "TipToast {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:1,"
        "    stop:0 #2d7d46, stop:1 #1e5f30);"
        "  border: 1px solid #4CAF50;"
        "  border-radius: 8px;"
        "  padding: 10px;"
        "}"
        "TipToast QLabel { color: white; }"
    );
    setFixedWidth(320);
    setMinimumHeight(60);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(4);

    // Top line: emoji + singer name + amount
    auto *topRow = new QHBoxLayout();
    topRow->setSpacing(6);

    auto *emojiLabel = new QLabel("🎉", this);
    emojiLabel->setStyleSheet("font-size: 18px;");
    topRow->addWidget(emojiLabel);

    auto *nameLabel = new QLabel(singerName, this);
    nameLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    topRow->addWidget(nameLabel);

    topRow->addStretch();

    QString amountStr;
    if (currency == "USD")
        amountStr = QString("$%1").arg(amountCents / 100.0, 0, 'f', 2);
    else if (currency == "EUR")
        amountStr = QString("€%1").arg(amountCents / 100.0, 0, 'f', 2);
    else if (currency == "GBP")
        amountStr = QString("£%1").arg(amountCents / 100.0, 0, 'f', 2);
    else
        amountStr = QString("%1 %2").arg(amountCents / 100.0, 0, 'f', 2).arg(currency);

    auto *amountLabel = new QLabel(amountStr, this);
    amountLabel->setStyleSheet("font-weight: bold; font-size: 16px; color: #A5D6A7;");
    topRow->addWidget(amountLabel);

    layout->addLayout(topRow);

    // Optional message
    if (!message.isEmpty()) {
        auto *msgLabel = new QLabel(QString("\"%1\"").arg(message), this);
        msgLabel->setStyleSheet("font-size: 12px; font-style: italic; color: #C8E6C9;");
        msgLabel->setWordWrap(true);
        layout->addWidget(msgLabel);
    }

    // Fade effect
    m_opacityEffect = new QGraphicsOpacityEffect(this);
    m_opacityEffect->setOpacity(1.0);
    setGraphicsEffect(m_opacityEffect);

    m_fadeAnim = new QPropertyAnimation(m_opacityEffect, "opacity", this);
    m_fadeAnim->setDuration(FADE_DURATION_MS);
    m_fadeAnim->setStartValue(1.0);
    m_fadeAnim->setEndValue(0.0);
    connect(m_fadeAnim, &QPropertyAnimation::finished, this, [this]() {
        emit dismissRequested();
    });

    // Click to dismiss immediately
    setCursor(Qt::PointingHandCursor);

    // Start fade timer
    QTimer::singleShot(FADE_DELAY_MS, this, &TipToast::startFadeOut);
}

void TipToast::startFadeOut()
{
    if (m_fadeAnim && m_fadeAnim->state() != QPropertyAnimation::Running)
        m_fadeAnim->start();
}

void TipToast::mousePressEvent(QMouseEvent *)
{
    // Immediate dismiss on click
    if (m_fadeAnim && m_fadeAnim->state() == QPropertyAnimation::Running)
        m_fadeAnim->stop();
    emit dismissRequested();
}

// ── TipPanel ─────────────────────────────────────────────────────────────────

TipPanel::TipPanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("TipPanel");
    setFixedWidth(WIDTH);
    setVisible(false);

    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(MARGIN, MARGIN, MARGIN, MARGIN);
    m_layout->setSpacing(6);
    m_layout->setDirection(QVBoxLayout::BottomToTop);

    // Gratitude label at the bottom of the stack
    m_gratitudeLabel = new QLabel("💵 Thanks for the tips!", this);
    m_gratitudeLabel->setStyleSheet(
        "font-size: 11px; color: #888; padding: 2px 0;"
    );
    m_gratitudeLabel->setAlignment(Qt::AlignCenter);
    m_gratitudeLabel->setVisible(false);
    m_layout->addWidget(m_gratitudeLabel);

    // Total label
    m_totalLabel = new QLabel("Tip Jar: $0.00", this);
    m_totalLabel->setStyleSheet(
        "font-weight: bold; font-size: 13px; color: #4CAF50;"
        " background: rgba(0,0,0,0.4); border-radius: 4px; padding: 4px 8px;"
    );
    m_totalLabel->setAlignment(Qt::AlignCenter);
    m_totalLabel->setFixedHeight(28);
    m_layout->addWidget(m_totalLabel);
}

void TipPanel::showTip(const QString &singerName, int amountCents,
                       const QString &message, const QString &currency)
{
    m_totalCents += amountCents;
    setTotalAmount(m_totalCents);

    auto *toast = new TipToast(singerName, amountCents, message, currency, this);
    connect(toast, &TipToast::dismissRequested, this, &TipPanel::onToastDismissed);

    if (m_activeToasts.size() < MAX_VISIBLE) {
        m_activeToasts.append(toast);
        m_layout->insertWidget(m_layout->count() - 2, toast); // before total + gratitude
        rearrange();
    } else {
        m_pendingToasts.enqueue(toast);
    }

    m_gratitudeLabel->setVisible(true);
    setVisible(true);
    raise();
}

void TipPanel::setTotalAmount(int totalCents)
{
    m_totalCents = totalCents;
    m_totalLabel->setText(QString("Tip Jar: %1").arg(formatCurrency(totalCents, "USD")));
}

void TipPanel::reset()
{
    // Dismiss all active toasts
    for (auto *toast : m_activeToasts) {
        toast->deleteLater();
    }
    m_activeToasts.clear();

    // Clear pending queue
    while (!m_pendingToasts.isEmpty())
        delete m_pendingToasts.dequeue();

    m_totalCents = 0;
    m_totalLabel->setText("Tip Jar: $0.00");
    m_gratitudeLabel->setVisible(false);
    setVisible(false);
}

void TipPanel::onToastDismissed()
{
    auto *toast = qobject_cast<TipToast *>(sender());
    if (!toast)
        return;

    m_activeToasts.removeOne(toast);
    toast->deleteLater();
    rearrange();

    // Show next pending if any
    if (!m_pendingToasts.isEmpty())
        showNextPending();

    // Hide panel when no toasts remain
    if (m_activeToasts.isEmpty()) {
        m_gratitudeLabel->setVisible(m_totalCents > 0);
        if (m_totalCents == 0)
            setVisible(false);
    }
}

void TipPanel::rearrange()
{
    // Reposition in bottom-right of parent
    if (parentWidget()) {
        int x = parentWidget()->width() - width() - 16;
        int y = parentWidget()->height() - height() - 16;
        move(x, y);
    }
}

void TipPanel::showNextPending()
{
    if (m_pendingToasts.isEmpty())
        return;

    auto *toast = m_pendingToasts.dequeue();
    m_activeToasts.append(toast);
    m_layout->insertWidget(m_layout->count() - 2, toast);
    rearrange();
}

void TipPanel::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    rearrange();
}

QString TipPanel::formatCurrency(int cents, const QString &currency) const
{
    if (currency == "USD")
        return QString("$%1").arg(cents / 100.0, 0, 'f', 2);
    if (currency == "EUR")
        return QString("€%1").arg(cents / 100.0, 0, 'f', 2);
    if (currency == "GBP")
        return QString("£%1").arg(cents / 100.0, 0, 'f', 2);
    return QString("%1 %2").arg(cents / 100.0, 0, 'f', 2).arg(currency);
}
