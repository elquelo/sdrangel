///////////////////////////////////////////////////////////////////////////////////
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDE_FEATURE_JOGDIALRANGESLIDER_H_
#define INCLUDE_FEATURE_JOGDIALRANGESLIDER_H_

#include <algorithm>
#include <functional>

#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QWidget>

//! Slider with two handles that selects a range of integer values. It never takes the keyboard focus.
//! The change is reported with the onChanged callback while a handle is dragged
class JogdialRangeSlider : public QWidget
{
public:
    explicit JogdialRangeSlider(QWidget *parent = nullptr) :
        QWidget(parent)
    {
        setFocusPolicy(Qt::NoFocus);
        setMinimumHeight(24);
    }

    void setBounds(int minimum, int maximum, int minGap)
    {
        m_min = minimum;
        m_max = std::max(minimum + 1, maximum);
        m_gap = minGap;
        setValues(m_low, m_high);
    }

    void setValues(int low, int high)
    {
        m_high = std::max(m_min + m_gap, std::min(m_max, high));
        m_low = std::max(m_min, std::min(m_high - m_gap, low));
        update();
    }

    int low() const { return m_low; }
    int high() const { return m_high; }

    std::function<void(int, int)> onChanged;

    QSize sizeHint() const override { return QSize(160, 24); }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const int cy = height() / 2;
        const int xl = xOf(m_low);
        const int xh = xOf(m_high);

        painter.setPen(Qt::NoPen);
        painter.setBrush(palette().color(QPalette::Mid));
        painter.drawRoundedRect(QRect(m_margin, cy - 2, width() - 2*m_margin, 4), 2, 2);
        painter.setBrush(palette().color(QPalette::Highlight));
        painter.drawRoundedRect(QRect(xl, cy - 2, xh - xl, 4), 2, 2);

        painter.setPen(QPen(palette().color(QPalette::Dark), 1));
        painter.setBrush(palette().color(isEnabled() ? QPalette::Light : QPalette::Midlight));
        painter.drawEllipse(QPoint(xl, cy), m_radius, m_radius);
        painter.drawEllipse(QPoint(xh, cy), m_radius, m_radius);
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        const int x = pointX(event);
        // The nearest handle moves; when they are stacked the side of the click decides
        const int dl = std::abs(x - xOf(m_low));
        const int dh = std::abs(x - xOf(m_high));
        m_dragged = (dl < dh) ? 1 : ((dh < dl) ? 2 : ((x < xOf(m_low)) ? 1 : 2));
        moveTo(x);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (m_dragged) {
            moveTo(pointX(event));
        }
    }

    void mouseReleaseEvent(QMouseEvent *) override
    {
        m_dragged = 0;
    }

private:
    int m_min{0};
    int m_max{100};
    int m_gap{1};
    int m_low{0};
    int m_high{100};
    int m_dragged{0}; //!< 0: none, 1: low handle, 2: high handle
    static const int m_margin = 10;
    static const int m_radius = 7;

    static int pointX(const QMouseEvent *event)
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        return (int) event->position().x();
#else
        return event->pos().x();
#endif
    }

    int xOf(int value) const
    {
        const int span = std::max(1, width() - 2*m_margin);
        return m_margin + (int) ((double) (value - m_min) / (m_max - m_min) * span);
    }

    void moveTo(int x)
    {
        const int span = std::max(1, width() - 2*m_margin);
        const int value = m_min + (int) std::lround((double) (x - m_margin) / span * (m_max - m_min));
        const int oldLow = m_low;
        const int oldHigh = m_high;

        if (m_dragged == 1) {
            setValues(std::min(value, m_high - m_gap), m_high);
        } else if (m_dragged == 2) {
            setValues(m_low, std::max(value, m_low + m_gap));
        }

        if (((m_low != oldLow) || (m_high != oldHigh)) && onChanged) {
            onChanged(m_low, m_high);
        }
    }
};

#endif // INCLUDE_FEATURE_JOGDIALRANGESLIDER_H_
