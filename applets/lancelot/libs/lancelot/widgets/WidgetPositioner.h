/*
 *   Copyright (C) 2007 Ivan Cukic <ivan.cukic+kde@gmail.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser/Library General Public License version 2,
 *   or (at your option) any later version, as published by the Free
 *   Software Foundation
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser/Library General Public License for more details
 *
 *   You should have received a copy of the GNU Lesser/Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef LANCELOT_WIDGET_POSITIONER_H
#define LANCELOT_WIDGET_POSITIONER_H

#include <lancelot/lancelot_export.h>

#include <lancelot/widgets/Widget.h>
#include <QtGui/QWidget>
#include <QtGui/QGraphicsView>

namespace Lancelot
{

/**
 * Dirty hack to show QWidget as a part of QGV. It works for the
 * case it is used. It is not meant to be used elsewhere.
 */
class LANCELOT_EXPORT_DEPRECATED WidgetPositioner: public Lancelot::Widget
{

    L_WIDGET
    L_INCLUDE(lancelot/widgets/WidgetPositioner.h)

public:
    WidgetPositioner(QWidget * widget, QGraphicsView * view, QGraphicsItem * parent);
    virtual ~WidgetPositioner();

    void setGeometry (const QRectF & geometry);
    QRectF geometry() const;

    QSizeF sizeHint() const;

private:
    QRectF m_geometry;
    QWidget * m_widget;
    QGraphicsView * m_view;
};

} // namespace Lancelot

#endif /* LANCELOT_WIDGET_POSITIONER_H */

