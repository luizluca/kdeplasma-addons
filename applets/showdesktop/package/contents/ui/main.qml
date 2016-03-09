/*
    Copyright (C) 2014 Ashish Madeti <ashishmadeti@gmail.com>
    Copyright (C) 2016 Kai Uwe Broulik <kde@privat.broulik.de>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

import QtQuick 2.1
import QtQuick.Layouts 1.1

import org.kde.plasma.core 2.0 as PlasmaCore

import org.kde.plasma.plasmoid 2.0

import org.kde.plasma.private.showdesktop 0.1

QtObject {
    id: root

    // you can't have an applet with just a compact representation :(
    Plasmoid.preferredRepresentation: Plasmoid.fullRepresentation
    Plasmoid.onActivated: showdesktop.showDesktop()
    Plasmoid.icon: plasmoid.configuration.icon
    Plasmoid.title: i18n("Show Desktop")
    Plasmoid.toolTipSubText: i18n("Show the Plasma desktop")

    // Bug 354257 add minimize all context menu entry

    // QtObject has no default property
    property QtObject showdesktop: ShowDesktop { }

    Plasmoid.fullRepresentation: PlasmaCore.ToolTipArea {
        Layout.minimumWidth: units.iconSizes.small
        Layout.minimumHeight: Layout.minimumWidth

        icon: plasmoid.icon
        mainText: plasmoid.title
        subText: plasmoid.toolTipSubText

        MouseArea {
            anchors.fill: parent
            onClicked: showdesktop.showDesktop()
        }

        PlasmaCore.IconItem {
            anchors.fill: parent
            source: plasmoid.icon
            active: parent.containsMouse
        }
    }

}
