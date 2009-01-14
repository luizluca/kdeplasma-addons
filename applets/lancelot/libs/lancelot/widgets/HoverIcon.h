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

#ifndef LANCELOT_HOVER_ICON_H
#define LANCELOT_HOVER_ICON_H

#include <plasma/widgets/iconwidget.h>
#include <lancelot/lancelot.h>
#include <lancelot/lancelot_export.h>
#include <lancelot/widgets/Widget.h>

namespace Lancelot
{

/**
 * Wrapper class for Plasma::Icon which adds hover activation.
 * This class is not a part of the standard Lancelot framework
 * and doesn't support groups yet.
 *
 * @author Ivan Cukic
 */
class LANCELOT_EXPORT HoverIcon: public Plasma::IconWidget {
    Q_OBJECT

    Q_PROPERTY ( ActivationMethod activationMethod READ activationMethod WRITE setActivationMethod )

    L_WIDGET
    L_INCLUDE(lancelot/widgets/HoverIcon.h plasma/widgets/icon.h)

public:
    /**
     * Creates a new Lancelot::HoverIcon
     * @param parent parent item
     */
    HoverIcon(QGraphicsItem * parent = 0);

    /**
     * Creates a new Lancelot::HoverIcon
     * @param text title
     * @param parent parent item
     */
    HoverIcon(const QString & text, QGraphicsItem * parent = 0);

    /**
     * Creates a new Lancelot::HoverIcon
     * @param icon icon
     * @param text title
     * @param parent parent item
     */
    HoverIcon(const QIcon & icon, const QString & text, QGraphicsItem * parent = 0);

    /**
     * Destroys this Lancelot::HoverIcon
     */
    ~HoverIcon();

    /**
     * Sets the activation method of the ExtenderButton.
     * Only hover and click activations are supported. In the
     * case of click activation, HoverIcon will follow the
     * global system settings concerning click vs double click.
     * @param method new activation method
     */
    void setActivationMethod(Lancelot::ActivationMethod method);

    /**
     * @returns activation method
     */
    Lancelot::ActivationMethod activationMethod();

    // L_Override virtual void groupUpdated();

protected:
    L_Override virtual void hoverEnterEvent(QGraphicsSceneHoverEvent * event);
    L_Override virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent * event);
    L_Override virtual void timerEvent(QTimerEvent * event);

private:
    class Private;
    Private * const d;
};

} // namespace Lancelot

#endif /* LANCELOT_HOVER_ICON_H */


