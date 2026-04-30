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

#ifndef TIPPANEL_H
#define TIPPANEL_H

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QPropertyAnimation>
#include <QQueue>
#include <QGraphicsOpacityEffect>
#include <QFrame>
#include <QMouseEvent>

/**
 * @brief Displays a single tip notification toast with auto-fade.
 *
 * Auto-dismisses after TipToast::FADE_DELAY_MS (5 seconds) unless
 * clicked. Stacked via TipPanel which manages multiple toasts.
 */
class TipToast : public QFrame
{
    Q_OBJECT
public:
    explicit TipToast(const QString &singerName, int amountCents,
                      const QString &message, const QString &currency,
                      QWidget *parent = nullptr);

    void startFadeOut();

protected:
    void mousePressEvent(QMouseEvent *event) override;

signals:
    void dismissRequested();

private:
    QGraphicsOpacityEffect *m_opacityEffect{nullptr};
    QPropertyAnimation *m_fadeAnim{nullptr};

    static constexpr int FADE_DELAY_MS = 5000;
    static constexpr int FADE_DURATION_MS = 500;
};

/**
 * @brief Manages a stack of tip toast notifications in the bottom-right
 *        corner of the parent widget.
 *
 * Shows up to MAX_VISIBLE toasts at once; extras are queued and displayed
 * as earlier ones disappear.
 */
class TipPanel : public QWidget
{
    Q_OBJECT
public:
    explicit TipPanel(QWidget *parent = nullptr);

    /** Add a new tip notification to the stack */
    void showTip(const QString &singerName, int amountCents,
                 const QString &message, const QString &currency = "USD");

    /** Update the total tip amount displayed */
    void setTotalAmount(int totalCents);

    /** Reset all state (new gig / new night) */
    void reset();

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onToastDismissed();

private:
    QVBoxLayout *m_layout;
    QLabel *m_totalLabel;
    QLabel *m_gratitudeLabel;
    QQueue<TipToast *> m_pendingToasts;
    QList<TipToast *> m_activeToasts;
    int m_totalCents{0};

    static constexpr int MAX_VISIBLE = 3;
    static constexpr int WIDTH = 320;
    static constexpr int MARGIN = 8;

    void rearrange();
    void showNextPending();
    QString formatCurrency(int cents, const QString &currency) const;
};

#endif // TIPPANEL_H
