/* Copyright (C) 2022 by Jeremy Tan */
/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.

 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.

 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FONTFORGE_GQTDRAWP_H
#define FONTFORGE_GQTDRAWP_H

#include <fontforge-config.h>

#ifdef FONTFORGE_CAN_USE_QT

#include "fontP.h"
#include "gdrawlogger.h"
#include "gdrawP.h"
#include "gkeysym.h"

#include <QtWidgets>
#include <vector>

namespace fontforge { namespace gdraw {

class GQtWindow;
class GQtPixmap;
class GQtWidget;

struct GQtButtonState {
    uint32_t last_press_time;
    GQtWindow *release_w;
    int16_t release_x, release_y;
    int16_t release_button;
    int16_t cur_click;
    int16_t double_time;		// max milliseconds between release & click
    int16_t double_wiggle;	// max pixel wiggle allowed between release&click
};

struct GQtLayoutState {
    std::vector<int> indexes;
    std::unique_ptr<QFontMetrics> metrics;
    QTextLayout layout;
};

struct GQtDisplay
{
    struct gdisplay base;
    std::unique_ptr<QApplication> app;
    std::vector<QCursor> custom_cursors;
    GQtWindow *default_icon = nullptr;

    bool is_space_pressed = false; // Used for GGDKDrawKeyState. We cheat!

    int top_window_count = 0; // The number of toplevel, non-dialogue windows. When this drops to 0, the event loop stops
    ulong last_event_time = 0;
    GQtWindow *grabbed_window = nullptr;

    FState fs = {};
    GQtButtonState bs = {};

    inline GDisplay* Base() const { return const_cast<GDisplay*>(&base); }
};

class GQtWindow
{
public:
    GQtWindow(bool is_pixmap)
    {
        m_ggc.clip.width = m_ggc.clip.height = 0x7fff;
        m_ggc.fg = 0;
        m_ggc.bg = 0xffffff;
        m_base.is_pixmap = is_pixmap;
        m_base.ggc = &m_ggc;
    }
    virtual ~GQtWindow() = default;

    inline GWindow Base() const { return const_cast<GWindow>(&m_base); }

    virtual GQtPixmap* Pixmap() { assert(false); return nullptr; }
    virtual GQtWidget* Widget() { assert(false); return nullptr; }
    virtual QPainter* Painter() = 0;

    inline const char* Title() const { return m_window_title.c_str(); }
    inline void SetTitle(const char *title) { m_window_title = title; }
    GCursor Cursor() const { return m_current_cursor; }
    void SetCursor(GCursor c) { m_current_cursor = c; }
    GQtLayoutState* InitLayout() { m_layout_state.reset(new GQtLayoutState()); return m_layout_state.get(); }
    GQtLayoutState* Layout() { return m_layout_state.get(); }

private:
    struct gwindow m_base = {};
    GGC m_ggc = {};

    std::string m_window_title;
    GCursor m_current_cursor = ct_default;
    std::unique_ptr<GQtLayoutState> m_layout_state;
};

class GQtWidget : public QWidget, public GQtWindow
{
public:
    explicit GQtWidget(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags())
        : QWidget(parent, f)
        , GQtWindow(false)
    {
    }

    ~GQtWidget() override;

    template<typename E = void>
    GEvent InitEvent(event_type et, E* event = nullptr);
    bool DispatchEvent(const GEvent& e);

    bool event(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override { mouseEvent(event, et_mousedown); }
    void mouseReleaseEvent(QMouseEvent *event) override { mouseEvent(event, et_mouseup); }
    void wheelEvent(QWheelEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override { keyEvent(event, et_char); }
    void keyReleaseEvent(QKeyEvent *event) override { keyEvent(event, et_charup); }
    void paintEvent(QPaintEvent *event) override;
    void moveEvent(QMoveEvent *event) override { configureEvent(); }
    void resizeEvent(QResizeEvent *event) override { configureEvent(); }
    void showEvent(QShowEvent *event) override { mapEvent(true); }
    void hideEvent(QHideEvent *event) override { mapEvent(false); }
    void focusInEvent(QFocusEvent *event) override {
        QWidget::focusInEvent(event);
        if (!event->isAccepted()) {
            focusEvent(event, true);
        }
    }
    void focusOutEvent(QFocusEvent *event) override {
        QWidget::focusOutEvent(event);
        if (!event->isAccepted()) {
            focusEvent(event, false);
        }
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void enterEvent(QEnterEvent *event) override { crossingEvent(event, true); }
#else
    void enterEvent(QEvent *event) override { crossingEvent(event, true); }
#endif
    void leaveEvent(QEvent *event) override { crossingEvent(event, false); }
    void closeEvent(QCloseEvent *event) override;

    void keyEvent(QKeyEvent *event, event_type et);
    void mouseEvent(QMouseEvent* event, event_type et);
    void configureEvent();
    void mapEvent(bool visible);
    void focusEvent(QFocusEvent* event, bool focusIn);
    void crossingEvent(QEvent* event, bool enter);
    

    GQtWidget* Widget() override { return this; }
    QPainter* Painter() override;

private:
    QPainter* m_painter = nullptr;
};

class GQtPixmap : public QPixmap, public GQtWindow
{
public:
    explicit GQtPixmap(int w, int h)
        : QPixmap(w, h)
        , GQtWindow(true)
    {
    }

    GQtPixmap* Pixmap() override { return this; }
    QPainter* Painter() override {
        if (!m_painter.isActive()) {
            m_painter.begin(this);
            // m_painter.setRenderHint(QPainter::Antialiasing);
            // m_painter.setRenderHint(QPainter::HighQualityAntialiasing);
        }
        return &m_painter;
    }

private:
    QPainter m_painter;
};

class GQtTimer : public QTimer
{
public:
    explicit GQtTimer(GQtWindow *parent, void *userdata);
    GTimer* Base() const { return const_cast<GTimer*>(&m_gtimer); }
private:
    GTimer m_gtimer;
};

static inline GQtDisplay* GQtD(GDisplay *d) {
    return static_cast<GQtDisplay*>(d->impl);
}

static inline GQtDisplay* GQtD(GWindow w) {
    return static_cast<GQtDisplay*>(w->display->impl);
}

static inline GQtDisplay* GQtD(GQtWindow* w) {
    return GQtD(w->Base());
}

static inline GQtWindow* GQtW(GWindow w) {
    return static_cast<GQtWindow*>(w->native_window);
}

}} // fontforge::gdraw

#endif // FONTFORGE_CAN_USE_QT

#endif /* FONTFORGE_GQTDRAWP_H */