/*
 * Auto-KJ Modern Theme System
 * Copyright (c) 2026 Auto-KJ Contributors
 *
 * Provides dark and light themes as Qt stylesheets (QSS),
 * replacing the old Fusion palette hack in main.cpp.
 *
 * Usage:
 *   ThemeEngine::apply(app, ThemeEngine::Dark);
 *   ThemeEngine::apply(app, ThemeEngine::Light);
 *   ThemeEngine::apply(app, ThemeEngine::System);
 */

#ifndef THEMEENGINE_H
#define THEMEENGINE_H

#include <QApplication>
#include <QPalette>
#include <QStyleFactory>
#include <QFile>
#include <QRegularExpression>
#include <QScreen>
#include <QGuiApplication>
#include <spdlog/spdlog.h>

class ThemeEngine {
public:
    enum Theme {
        System = 0,
        Dark = 1,
        Light = 2
    };

    static void apply(QApplication &app, Theme theme) {
        QApplication::setStyle(QStyleFactory::create("Fusion"));

        switch (theme) {
        case Dark:
            applyDark(app);
            break;
        case Light:
            applyLight(app);
            break;
        case System:
        default:
            applySystem(app);
            break;
        }

        spdlog::get("logger")->info("[ThemeEngine] Applied theme: {}",
                                    themeName(theme).toStdString());
    }

    static QString themeName(Theme theme) {
        switch (theme) {
        case Dark:   return "Dark";
        case Light:  return "Light";
        case System: return "System";
        default:     return "Unknown";
        }
    }

    static bool isDark(Theme theme) {
        if (theme == Dark) return true;
        if (theme == Light) return false;
        // System: check system color scheme
        return QGuiApplication::palette().window().color().lightness() < 128;
    }

private:
    static void applySystem(QApplication &app) {
        // Detect system preference
        bool systemIsDark = QGuiApplication::palette().window().color().lightness() < 128;
        if (systemIsDark)
            applyDark(app);
        else
            applyLight(app);
    }

    static void applyDark(QApplication &app) {
        QPalette p;
        p.setColor(QPalette::Window,          QColor(30, 30, 30));
        p.setColor(QPalette::WindowText,      QColor(220, 220, 220));
        p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(110, 110, 110));
        p.setColor(QPalette::Base,            QColor(22, 22, 22));
        p.setColor(QPalette::AlternateBase,   QColor(40, 40, 40));
        p.setColor(QPalette::ToolTipBase,     QColor(50, 50, 50));
        p.setColor(QPalette::ToolTipText,     QColor(220, 220, 220));
        p.setColor(QPalette::Text,            QColor(220, 220, 220));
        p.setColor(QPalette::Disabled, QPalette::Text, QColor(110, 110, 110));
        p.setColor(QPalette::Dark,            QColor(18, 18, 18));
        p.setColor(QPalette::Shadow,          QColor(10, 10, 10));
        p.setColor(QPalette::Button,          QColor(42, 42, 42));
        p.setColor(QPalette::ButtonText,      QColor(220, 220, 220));
        p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(110, 110, 110));
        p.setColor(QPalette::BrightText,      QColor(255, 50, 50));
        p.setColor(QPalette::Link,            QColor(42, 130, 218));
        p.setColor(QPalette::Highlight,       QColor(42, 130, 218));
        p.setColor(QPalette::Disabled, QPalette::Highlight, QColor(60, 60, 60));
        p.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
        p.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(110, 110, 110));
        app.setPalette(p);

        app.setStyleSheet(darkStylesheet());
    }

    static void applyLight(QApplication &app) {
        QPalette p;
        p.setColor(QPalette::Window,          QColor(245, 245, 245));
        p.setColor(QPalette::WindowText,      QColor(30, 30, 30));
        p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(140, 140, 140));
        p.setColor(QPalette::Base,            QColor(255, 255, 255));
        p.setColor(QPalette::AlternateBase,   QColor(238, 238, 238));
        p.setColor(QPalette::ToolTipBase,     QColor(255, 255, 255));
        p.setColor(QPalette::ToolTipText,      QColor(30, 30, 30));
        p.setColor(QPalette::Text,            QColor(30, 30, 30));
        p.setColor(QPalette::Disabled, QPalette::Text, QColor(140, 140, 140));
        p.setColor(QPalette::Dark,            QColor(200, 200, 200));
        p.setColor(QPalette::Shadow,          QColor(160, 160, 160));
        p.setColor(QPalette::Button,          QColor(240, 240, 240));
        p.setColor(QPalette::ButtonText,      QColor(30, 30, 30));
        p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(140, 140, 140));
        p.setColor(QPalette::BrightText,      QColor(255, 0, 0));
        p.setColor(QPalette::Link,            QColor(42, 130, 218));
        p.setColor(QPalette::Highlight,       QColor(42, 130, 218));
        p.setColor(QPalette::Disabled, QPalette::Highlight, QColor(200, 200, 200));
        p.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
        p.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(140, 140, 140));
        app.setPalette(p);

        app.setStyleSheet(lightStylesheet());
    }

    // ── Dark Theme Stylesheet ──────────────────────────────────────────────
    static QString darkStylesheet() {
        return R"QSS(

/* ─── Base ─────────────────────────────────────────────────────────────── */
QWidget {
    font-size: 10pt;
}

QMainWindow {
    background: #1e1e1e;
}

/* ─── Group Boxes ──────────────────────────────────────────────────────── */
QGroupBox {
    border: 1px solid #3a3a3a;
    border-radius: 6px;
    margin-top: 12px;
    padding: 12px 8px 8px 8px;
    font-weight: bold;
}
QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    padding: 2px 8px;
    color: #2a82da;
}

/* ─── Tabs ─────────────────────────────────────────────────────────────── */
QTabWidget::pane {
    border: 1px solid #3a3a3a;
    border-radius: 4px;
    background: #1e1e1e;
}
QTabBar::tab {
    padding: 8px 16px;
    border: 1px solid #3a3a3a;
    border-bottom: none;
    border-top-left-radius: 6px;
    border-top-right-radius: 6px;
    background: #2a2a2a;
    color: #ccc;
    min-width: 60px;
}
QTabBar::tab:selected {
    background: #1e1e1e;
    color: #fff;
    border-bottom: 2px solid #2a82da;
}
QTabBar::tab:hover:!selected {
    background: #333;
}
QTabWidget::tab-bar {
    alignment: left;
}

/* West-position tabs (like the main window) */
QTabWidget[tabPosition="1"]::pane {
    border-left: 1px solid #3a3a3a;
    border-radius: 0;
}
QTabWidget[tabPosition="1"] QTabBar::tab {
    border: 1px solid #3a3a3a;
    border-right: none;
    border-top-left-radius: 6px;
    border-bottom-left-radius: 6px;
    border-top-right-radius: 0;
    border-bottom-right-radius: 0;
    padding: 8px 12px;
}

/* ─── Buttons ──────────────────────────────────────────────────────────── */
QPushButton {
    background: #2e2e2e;
    border: 1px solid #3a3a3a;
    border-radius: 5px;
    padding: 6px 16px;
    color: #ddd;
    min-height: 22px;
}
QPushButton:hover {
    background: #3a3a3a;
    border-color: #2a82da;
}
QPushButton:pressed {
    background: #2a82da;
    color: #fff;
}
QPushButton:disabled {
    background: #252525;
    color: #666;
    border-color: #2a2a2a;
}
QPushButton:checked {
    background: #2a82da;
    color: #fff;
    border-color: #2a82da;
}

QToolButton {
    background: transparent;
    border: 1px solid transparent;
    border-radius: 4px;
    padding: 4px 8px;
    color: #ccc;
}
QToolButton:hover {
    background: #3a3a3a;
    border-color: #3a3a3a;
}
QToolButton:pressed {
    background: #2a82da;
    color: #fff;
}
QToolButton:checked {
    background: #2a82da;
    color: #fff;
}

/* ─── Line Edits ───────────────────────────────────────────────────────── */
QLineEdit {
    background: #161616;
    border: 1px solid #3a3a3a;
    border-radius: 4px;
    padding: 5px 8px;
    color: #ddd;
    selection-background-color: #2a82da;
}
QLineEdit:focus {
    border-color: #2a82da;
}

/* ─── Combo Boxes ──────────────────────────────────────────────────────── */
QComboBox {
    background: #2e2e2e;
    border: 1px solid #3a3a3a;
    border-radius: 4px;
    padding: 5px 8px;
    color: #ddd;
    min-height: 22px;
}
QComboBox:hover {
    border-color: #2a82da;
}
QComboBox::drop-down {
    border: none;
    width: 24px;
}
QComboBox QAbstractItemView {
    background: #2a2a2a;
    border: 1px solid #3a3a3a;
    selection-background-color: #2a82da;
    color: #ddd;
    outline: none;
}

/* ─── Spin Boxes ──────────────────────────────────────────────────────── */
QSpinBox, QDoubleSpinBox {
    background: #161616;
    border: 1px solid #3a3a3a;
    border-radius: 4px;
    padding: 5px 8px;
    color: #ddd;
}
QSpinBox:focus, QDoubleSpinBox:focus {
    border-color: #2a82da;
}

/* ─── Checkboxes & Radio ───────────────────────────────────────────────── */
QCheckBox, QRadioButton {
    spacing: 8px;
    color: #ddd;
}
QCheckBox::indicator, QRadioButton::indicator {
    width: 16px;
    height: 16px;
    border: 1px solid #3a3a3a;
    border-radius: 3px;
    background: #161616;
}
QRadioButton::indicator {
    border-radius: 8px;
}
QCheckBox::indicator:checked, QRadioButton::indicator:checked {
    background: #2a82da;
    border-color: #2a82da;
}
QCheckBox::indicator:hover, QRadioButton::indicator:hover {
    border-color: #555;
}

/* ─── Table Views ──────────────────────────────────────────────────────── */
QTableView, QListView {
    background: #161616;
    alternate-background-color: #1c1c1c;
    border: 1px solid #3a3a3a;
    border-radius: 4px;
    gridline-color: #2a2a2a;
    selection-background-color: #2a82da;
    selection-color: #fff;
    outline: none;
}
QHeaderView::section {
    background: #2a2a2a;
    color: #ccc;
    border: none;
    border-bottom: 1px solid #3a3a3a;
    padding: 6px 8px;
    font-weight: bold;
}
QHeaderView::section:hover {
    background: #333;
}

/* ─── Tree Views ───────────────────────────────────────────────────────── */
QTreeView {
    background: #161616;
    alternate-background-color: #1c1c1c;
    border: 1px solid #3a3a3a;
    border-radius: 4px;
    selection-background-color: #2a82da;
    selection-color: #fff;
    outline: none;
}

/* ─── Scroll Bars ─────────────────────────────────────────────────────── */
QScrollBar:vertical {
    background: #1e1e1e;
    width: 10px;
    margin: 0;
}
QScrollBar::handle:vertical {
    background: #444;
    min-height: 30px;
    border-radius: 5px;
}
QScrollBar::handle:vertical:hover {
    background: #555;
}
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
    height: 0;
}
QScrollBar:horizontal {
    background: #1e1e1e;
    height: 10px;
    margin: 0;
}
QScrollBar::handle:horizontal {
    background: #444;
    min-width: 30px;
    border-radius: 5px;
}
QScrollBar::handle:horizontal:hover {
    background: #555;
}
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
    width: 0;
}

/* ─── Sliders ──────────────────────────────────────────────────────────── */
QSlider::groove:horizontal {
    border: 1px solid #3a3a3a;
    height: 6px;
    background: #161616;
    border-radius: 3px;
}
QSlider::handle:horizontal {
    background: #2a82da;
    border: none;
    width: 16px;
    margin: -5px 0;
    border-radius: 8px;
}
QSlider::handle:horizontal:hover {
    background: #3a9aee;
}
QSlider::groove:vertical {
    border: 1px solid #3a3a3a;
    width: 6px;
    background: #161616;
    border-radius: 3px;
}
QSlider::handle:vertical {
    background: #2a82da;
    border: none;
    height: 16px;
    margin: 0 -5px;
    border-radius: 8px;
}
QSlider::handle:vertical:hover {
    background: #3a9aee;
}

/* ─── Progress Bars ────────────────────────────────────────────────────── */
QProgressBar {
    border: 1px solid #3a3a3a;
    border-radius: 4px;
    background: #161616;
    height: 8px;
    text-align: center;
    color: transparent;
}
QProgressBar::chunk {
    background: #2a82da;
    border-radius: 3px;
}

/* ─── Splitter ─────────────────────────────────────────────────────────── */
QSplitter::handle {
    background: #3a3a3a;
}
QSplitter::handle:horizontal {
    width: 2px;
}
QSplitter::handle:vertical {
    height: 2px;
}
QSplitter::handle:hover {
    background: #2a82da;
}

/* ─── Tooltips ─────────────────────────────────────────────────────────── */
QToolTip {
    background: #2a2a2a;
    color: #ddd;
    border: 1px solid #3a3a3a;
    padding: 6px;
    border-radius: 4px;
}

/* ─── Menu Bar & Menus ─────────────────────────────────────────────────── */
QMenuBar {
    background: #1e1e1e;
    color: #ccc;
    border-bottom: 1px solid #2a2a2a;
}
QMenuBar::item:selected {
    background: #2a82da;
    color: #fff;
}
QMenu {
    background: #2a2a2a;
    color: #ddd;
    border: 1px solid #3a3a3a;
    border-radius: 4px;
    padding: 4px;
}
QMenu::item {
    padding: 6px 24px;
    border-radius: 3px;
}
QMenu::item:selected {
    background: #2a82da;
    color: #fff;
}
QMenu::separator {
    height: 1px;
    background: #3a3a3a;
    margin: 4px 8px;
}

/* ─── Status Bar ───────────────────────────────────────────────────────── */
QStatusBar {
    background: #1e1e1e;
    color: #999;
    border-top: 1px solid #2a2a2a;
}

/* ─── Dialogs ──────────────────────────────────────────────────────────── */
QDialog {
    background: #1e1e1e;
}

/* ─── Text Edits ────────────────────────────────────────────────────────── */
QTextEdit, QPlainTextEdit {
    background: #161616;
    border: 1px solid #3a3a3a;
    border-radius: 4px;
    color: #ddd;
    selection-background-color: #2a82da;
}

/* ─── Label ─────────────────────────────────────────────────────────────── */
QLabel {
    color: #ddd;
}

/* ─── Frame ─────────────────────────────────────────────────────────────── */
QFrame {
    border-color: #3a3a3a;
}

)QSS";
    }

    // ── Light Theme Stylesheet ─────────────────────────────────────────────
    static QString lightStylesheet() {
        return R"QSS(

/* ─── Base ─────────────────────────────────────────────────────────────── */
QWidget {
    font-size: 10pt;
}

QMainWindow {
    background: #f5f5f5;
}

/* ─── Group Boxes ──────────────────────────────────────────────────────── */
QGroupBox {
    border: 1px solid #d0d0d0;
    border-radius: 6px;
    margin-top: 12px;
    padding: 12px 8px 8px 8px;
    font-weight: bold;
}
QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    padding: 2px 8px;
    color: #2a82da;
}

/* ─── Tabs ─────────────────────────────────────────────────────────────── */
QTabWidget::pane {
    border: 1px solid #d0d0d0;
    border-radius: 4px;
    background: #f5f5f5;
}
QTabBar::tab {
    padding: 8px 16px;
    border: 1px solid #d0d0d0;
    border-bottom: none;
    border-top-left-radius: 6px;
    border-top-right-radius: 6px;
    background: #e8e8e8;
    color: #333;
    min-width: 60px;
}
QTabBar::tab:selected {
    background: #f5f5f5;
    color: #000;
    border-bottom: 2px solid #2a82da;
}
QTabBar::tab:hover:!selected {
    background: #ddd;
}
QTabWidget::tab-bar {
    alignment: left;
}

QTabWidget[tabPosition="1"]::pane {
    border-left: 1px solid #d0d0d0;
    border-radius: 0;
}
QTabWidget[tabPosition="1"] QTabBar::tab {
    border: 1px solid #d0d0d0;
    border-right: none;
    border-top-left-radius: 6px;
    border-bottom-left-radius: 6px;
    border-top-right-radius: 0;
    border-bottom-right-radius: 0;
    padding: 8px 12px;
}

/* ─── Buttons ──────────────────────────────────────────────────────────── */
QPushButton {
    background: #fff;
    border: 1px solid #c0c0c0;
    border-radius: 5px;
    padding: 6px 16px;
    color: #333;
    min-height: 22px;
}
QPushButton:hover {
    background: #e8e8e8;
    border-color: #2a82da;
}
QPushButton:pressed {
    background: #2a82da;
    color: #fff;
}
QPushButton:disabled {
    background: #f0f0f0;
    color: #aaa;
    border-color: #ddd;
}
QPushButton:checked {
    background: #2a82da;
    color: #fff;
    border-color: #2a82da;
}

QToolButton {
    background: transparent;
    border: 1px solid transparent;
    border-radius: 4px;
    padding: 4px 8px;
    color: #555;
}
QToolButton:hover {
    background: #e8e8e8;
    border-color: #d0d0d0;
}
QToolButton:pressed {
    background: #2a82da;
    color: #fff;
}
QToolButton:checked {
    background: #2a82da;
    color: #fff;
}

/* ─── Line Edits ───────────────────────────────────────────────────────── */
QLineEdit {
    background: #fff;
    border: 1px solid #c0c0c0;
    border-radius: 4px;
    padding: 5px 8px;
    color: #333;
    selection-background-color: #2a82da;
}
QLineEdit:focus {
    border-color: #2a82da;
}

/* ─── Combo Boxes ──────────────────────────────────────────────────────── */
QComboBox {
    background: #fff;
    border: 1px solid #c0c0c0;
    border-radius: 4px;
    padding: 5px 8px;
    color: #333;
    min-height: 22px;
}
QComboBox:hover {
    border-color: #2a82da;
}
QComboBox::drop-down {
    border: none;
    width: 24px;
}
QComboBox QAbstractItemView {
    background: #fff;
    border: 1px solid #c0c0c0;
    selection-background-color: #2a82da;
    selection-color: #fff;
    outline: none;
}

/* ─── Spin Boxes ──────────────────────────────────────────────────────── */
QSpinBox, QDoubleSpinBox {
    background: #fff;
    border: 1px solid #c0c0c0;
    border-radius: 4px;
    padding: 5px 8px;
    color: #333;
}
QSpinBox:focus, QDoubleSpinBox:focus {
    border-color: #2a82da;
}

/* ─── Checkboxes & Radio ───────────────────────────────────────────────── */
QCheckBox, QRadioButton {
    spacing: 8px;
    color: #333;
}
QCheckBox::indicator, QRadioButton::indicator {
    width: 16px;
    height: 16px;
    border: 1px solid #c0c0c0;
    border-radius: 3px;
    background: #fff;
}
QRadioButton::indicator {
    border-radius: 8px;
}
QCheckBox::indicator:checked, QRadioButton::indicator:checked {
    background: #2a82da;
    border-color: #2a82da;
}
QCheckBox::indicator:hover, QRadioButton::indicator:hover {
    border-color: #999;
}

/* ─── Table Views ──────────────────────────────────────────────────────── */
QTableView, QListView {
    background: #fff;
    alternate-background-color: #f5f5f5;
    border: 1px solid #d0d0d0;
    border-radius: 4px;
    gridline-color: #e0e0e0;
    selection-background-color: #2a82da;
    selection-color: #fff;
    outline: none;
}
QHeaderView::section {
    background: #e8e8e8;
    color: #333;
    border: none;
    border-bottom: 1px solid #c0c0c0;
    padding: 6px 8px;
    font-weight: bold;
}
QHeaderView::section:hover {
    background: #ddd;
}

/* ─── Tree Views ───────────────────────────────────────────────────────── */
QTreeView {
    background: #fff;
    alternate-background-color: #f5f5f5;
    border: 1px solid #d0d0d0;
    border-radius: 4px;
    selection-background-color: #2a82da;
    selection-color: #fff;
    outline: none;
}

/* ─── Scroll Bars ─────────────────────────────────────────────────────── */
QScrollBar:vertical {
    background: #f5f5f5;
    width: 10px;
    margin: 0;
}
QScrollBar::handle:vertical {
    background: #bbb;
    min-height: 30px;
    border-radius: 5px;
}
QScrollBar::handle:vertical:hover {
    background: #999;
}
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
    height: 0;
}
QScrollBar:horizontal {
    background: #f5f5f5;
    height: 10px;
    margin: 0;
}
QScrollBar::handle:horizontal {
    background: #bbb;
    min-width: 30px;
    border-radius: 5px;
}
QScrollBar::handle:horizontal:hover {
    background: #999;
}
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
    width: 0;
}

/* ─── Sliders ──────────────────────────────────────────────────────────── */
QSlider::groove:horizontal {
    border: 1px solid #c0c0c0;
    height: 6px;
    background: #e0e0e0;
    border-radius: 3px;
}
QSlider::handle:horizontal {
    background: #2a82da;
    border: none;
    width: 16px;
    margin: -5px 0;
    border-radius: 8px;
}
QSlider::handle:horizontal:hover {
    background: #3a9aee;
}
QSlider::groove:vertical {
    border: 1px solid #c0c0c0;
    width: 6px;
    background: #e0e0e0;
    border-radius: 3px;
}
QSlider::handle:vertical {
    background: #2a82da;
    border: none;
    height: 16px;
    margin: 0 -5px;
    border-radius: 8px;
}
QSlider::handle:vertical:hover {
    background: #3a9aee;
}

/* ─── Progress Bars ────────────────────────────────────────────────────── */
QProgressBar {
    border: 1px solid #c0c0c0;
    border-radius: 4px;
    background: #e0e0e0;
    height: 8px;
    text-align: center;
    color: transparent;
}
QProgressBar::chunk {
    background: #2a82da;
    border-radius: 3px;
}

/* ─── Splitter ─────────────────────────────────────────────────────────── */
QSplitter::handle {
    background: #d0d0d0;
}
QSplitter::handle:horizontal {
    width: 2px;
}
QSplitter::handle:vertical {
    height: 2px;
}
QSplitter::handle:hover {
    background: #2a82da;
}

/* ─── Tooltips ─────────────────────────────────────────────────────────── */
QToolTip {
    background: #fff;
    color: #333;
    border: 1px solid #c0c0c0;
    padding: 6px;
    border-radius: 4px;
}

/* ─── Menu Bar & Menus ─────────────────────────────────────────────────── */
QMenuBar {
    background: #f5f5f5;
    color: #333;
    border-bottom: 1px solid #d0d0d0;
}
QMenuBar::item:selected {
    background: #2a82da;
    color: #fff;
}
QMenu {
    background: #fff;
    color: #333;
    border: 1px solid #d0d0d0;
    border-radius: 4px;
    padding: 4px;
}
QMenu::item {
    padding: 6px 24px;
    border-radius: 3px;
}
QMenu::item:selected {
    background: #2a82da;
    color: #fff;
}
QMenu::separator {
    height: 1px;
    background: #d0d0d0;
    margin: 4px 8px;
}

/* ─── Status Bar ───────────────────────────────────────────────────────── */
QStatusBar {
    background: #f5f5f5;
    color: #666;
    border-top: 1px solid #d0d0d0;
}

/* ─── Dialogs ──────────────────────────────────────────────────────────── */
QDialog {
    background: #f5f5f5;
}

/* ─── Text Edits ────────────────────────────────────────────────────────── */
QTextEdit, QPlainTextEdit {
    background: #fff;
    border: 1px solid #c0c0c0;
    border-radius: 4px;
    color: #333;
    selection-background-color: #2a82da;
}

/* ─── Label ─────────────────────────────────────────────────────────────── */
QLabel {
    color: #333;
}

/* ─── Frame ─────────────────────────────────────────────────────────────── */
QFrame {
    border-color: #d0d0d0;
}

)QSS";
    }
};

#endif // THEMEENGINE_H