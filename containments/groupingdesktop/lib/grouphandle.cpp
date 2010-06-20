/*
 *   Copyright 2007 by Kevin Ottens <ervin@kde.org>
 *   Copyright 2009 by Giulio Camuffo <giuliocamuffo@gmail.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "grouphandle.h"

#include <QtGui/QApplication>
#include <QtGui/QBitmap>
#include <QtGui/QGraphicsSceneMouseEvent>
#include <QtGui/QLinearGradient>
#include <QtGui/QPainter>
#include <QtGui/QApplication>
#include <QtGui/QMenu>

#include <kcolorscheme.h>
#include <kglobalsettings.h>
#include <kiconloader.h>
#include <kwindowsystem.h>

#include <cmath>
#include <math.h>

#include <Plasma/Containment>
#include <Plasma/Corona>
#include <Plasma/PaintUtils>
#include <plasma/theme.h>
#include <Plasma/View>
#include <Plasma/FrameSvg>
#include <Plasma/Animator>

#include "groupingcontainment.h"
#include "abstractgroup.h"

qreal _k_distanceForPoint(QPointF point);
qreal _k_pointAngle(QPointF in);
QPointF _k_rotatePoint(QPointF in, qreal rotateAngle);

GroupHandle::GroupHandle(GroupingContainment *parent, AbstractGroup *group, const QPointF &hoverPos)
    : QGraphicsObject(group),
      m_pressedButton(NoButton),
      m_containment(parent),
      m_group(group),
      m_iconSize(KIconLoader::SizeSmall),
      m_opacity(0.0),
      m_anim(FadeIn),
      m_animId(0),
      m_angle(0.0),
      m_backgroundBuffer(0),
      m_currentView(group->view()),
      m_entryPos(hoverPos),
      m_buttonsOnRight(false),
      m_pendingFade(false)
{
    setFlags(flags() | QGraphicsItem::ItemStacksBehindParent);
    KColorScheme colorScheme(QPalette::Active, KColorScheme::View,
                             Plasma::Theme::defaultTheme()->colorScheme());
    m_gradientColor = colorScheme.background(KColorScheme::NormalBackground).color();
    m_originalGeom = m_group->geometry();
    m_originalTransform = m_group->transform();

    m_hoverTimer = new QTimer(this);
    m_hoverTimer->setSingleShot(true);
    m_hoverTimer->setInterval(333);

    m_leaveTimer = new QTimer(this);
    m_leaveTimer->setSingleShot(true);
    m_leaveTimer->setInterval(500);

    connect(m_hoverTimer, SIGNAL(timeout()), this, SLOT(hoverTimeout()));
    connect(m_leaveTimer, SIGNAL(timeout()), this, SLOT(leaveTimeout()));
    connect(m_group, SIGNAL(groupDestroyed(AbstractGroup *)), this, SLOT(groupDestroyed()));

    setAcceptsHoverEvents(true);
    m_hoverTimer->start();

    //icons
    m_configureIcons = new Plasma::Svg(this);
    m_configureIcons->setImagePath("widgets/configuration-icons");
    m_configureIcons->setContainsMultipleImages(true);

    m_background = new Plasma::FrameSvg(this);
    m_background->setImagePath("widgets/background");
    m_group->installSceneEventFilter(this);
}

GroupHandle::~GroupHandle()
{
    detachGroup();
    delete m_backgroundBuffer;
}

AbstractGroup *GroupHandle::group() const
{
    return m_group;
}

void GroupHandle::detachGroup()
{
    if (!m_group) {
        return;
    }

    disconnect(m_hoverTimer, SIGNAL(timeout()), this, SLOT(hoverTimeout()));
    disconnect(m_leaveTimer, SIGNAL(timeout()), this, SLOT(leaveTimeout()));
    m_group->disconnect(this);

    if (m_group->geometry() != m_originalGeom || m_group->transform() != m_originalTransform) {
        emit m_group->groupTransformedByUser();
    }

    m_group = 0;
}

QRectF GroupHandle::boundingRect() const
{
    return m_totalRect;
}

QPainterPath GroupHandle::shape() const
{
    //when the containment changes the applet is reset to 0
    if (m_group) {
        QPainterPath path = Plasma::PaintUtils::roundedRectangle(m_decorationRect, 10);
        return path.united(m_group->shape());
    } else {
        return QGraphicsItem::shape();
    }
}

QPainterPath handleRect(const QRectF &rect, int radius, bool onRight)
{
    QPainterPath path;
    if (onRight) {
        // make the left side straight
        path.moveTo(rect.left(), rect.top());              // Top left
        path.lineTo(rect.right() - radius, rect.top());    // Top side
        path.quadTo(rect.right(), rect.top(),
                    rect.right(), rect.top() + radius);    // Top right corner
        path.lineTo(rect.right(), rect.bottom() - radius); // Right side
        path.quadTo(rect.right(), rect.bottom(),
                    rect.right() - radius, rect.bottom()); // Bottom right corner
        path.lineTo(rect.left(), rect.bottom());           // Bottom side
    } else {
        // make the right side straight
        path.moveTo(QPointF(rect.left(), rect.top() + radius));
        path.quadTo(rect.left(), rect.top(),
                    rect.left() + radius, rect.top());     // Top left corner
        path.lineTo(rect.right(), rect.top());             // Top side
        path.lineTo(rect.right(), rect.bottom());          // Right side
        path.lineTo(rect.left() + radius, rect.bottom());  // Bottom side
        path.quadTo(rect.left(), rect.bottom(),
                    rect.left(), rect.bottom() - radius);  // Bottom left corner
    }

    path.closeSubpath();
    return path;
}

void GroupHandle::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    //kDebug() << m_opacity << m_anim << FadeOut;
    if (qFuzzyCompare(m_opacity + 1.0, 1.0)) {
        if (m_anim == FadeOut) {
            //kDebug() << "WOOOOOOOOO";
            QTimer::singleShot(0, this, SLOT(emitDisappear()));
        }
        return;
    }

    qreal translation;

    if (m_buttonsOnRight) {
        //kDebug() << "translating by" << m_opacity
        //         << (-(1 - m_opacity) * m_rect.width()) << m_rect.width();
        translation = -(1 - m_opacity) * m_rect.width();
    } else {
        translation = (1 - m_opacity) * m_rect.width();
    }

    painter->translate(translation, 0);

    painter->setPen(Qt::NoPen);
    painter->setRenderHints(QPainter::Antialiasing);

    int iconMargin = m_iconSize / 2;

    const QSize pixmapSize(int(m_decorationRect.width()),
                           int(m_decorationRect.height()) + m_iconSize * 5 + 1);
    const QSize iconSize(KIconLoader::SizeSmall, KIconLoader::SizeSmall);

    //regenerate our buffer?
    if (m_animId > 0 || !m_backgroundBuffer || m_backgroundBuffer->size() != pixmapSize) {
        QColor transparencyColor = Qt::black;
        transparencyColor.setAlphaF(qMin(m_opacity, qreal(0.99)));

        QLinearGradient g(QPoint(0, 0), QPoint(m_decorationRect.width(), 0));
        //fading out panel
        if (m_rect.height() > qreal(minimumHeight()) * 1.25) {
            if (m_buttonsOnRight) {
                qreal opaquePoint =
                    (m_background->marginSize(Plasma::LeftMargin) - translation) / m_decorationRect.width();
                //kDebug() << "opaquePoint" << opaquePoint
                //         << m_background->marginSize(LeftMargin) << m_decorationRect.width();
                g.setColorAt(0.0, Qt::transparent);
                g.setColorAt(qMax(0.0, opaquePoint - 0.05), Qt::transparent);
                g.setColorAt(opaquePoint, transparencyColor);
                g.setColorAt(1.0, transparencyColor);
            } else {
                qreal opaquePoint =
                    1 - ((m_background->marginSize(Plasma::RightMargin) + translation) / m_decorationRect.width());
                g.setColorAt(1.0, Qt::transparent);
                g.setColorAt(opaquePoint + 0.05, Qt::transparent);
                g.setColorAt(qMax(0.0, opaquePoint), transparencyColor);
                g.setColorAt(0.0, transparencyColor);
            }
        //complete panel
        } else {
            g.setColorAt(0.0, transparencyColor);
        }

        m_background->resizeFrame(m_decorationRect.size());

        if (!m_backgroundBuffer || m_backgroundBuffer->size() != pixmapSize) {
            delete m_backgroundBuffer;
            m_backgroundBuffer = new QPixmap(pixmapSize);
        }
        m_backgroundBuffer->fill(Qt::transparent);
        QPainter buffPainter(m_backgroundBuffer);

        m_background->paintFrame(&buffPainter);

        //+1 because otherwise due to rounding errors when rotated could appear one pixel
        //of the icon at the border of the applet
        //QRectF iconRect(QPointF(pixmapSize.width() - m_iconSize + 1, m_iconSize), iconSize);
        QRectF iconRect(QPointF(0, m_decorationRect.height() + 1), iconSize);
        if (m_buttonsOnRight) {
            iconRect.moveLeft(
                pixmapSize.width() - m_iconSize - m_background->marginSize(Plasma::LeftMargin));
            m_configureIcons->paint(&buffPainter, iconRect, "size-diagonal-tr2bl");
        } else {
            iconRect.moveLeft(m_background->marginSize(Plasma::RightMargin));
            m_configureIcons->paint(&buffPainter, iconRect, "size-diagonal-tl2br");
        }

        iconRect.translate(0, m_iconSize);
        m_configureIcons->paint(&buffPainter, iconRect, "rotate");

//         if (m_group && m_group->hasConfigurationInterface()) {
//             iconRect.translate(0, m_iconSize);
//             m_configureIcons->paint(&buffPainter, iconRect, "configure");
//         }

        iconRect.translate(0, m_iconSize);
        m_configureIcons->paint(&buffPainter, iconRect, "close");

        buffPainter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        //blend the background
        buffPainter.fillRect(m_backgroundBuffer->rect(), g);
        //blend the icons
        //buffPainter.fillRect(QRect(QPoint((int)m_decorationRect.width(), 0), QSize(m_iconSize + 1,
        //                                  (int)m_decorationRect.height())), transparencyColor);
    }

    painter->drawPixmap(m_decorationRect.toRect(), *m_backgroundBuffer,
                        QRect(QPoint(0, 0), m_decorationRect.size().toSize()));

    //XXX this code is duplicated in the next function
    QPointF basePoint = m_rect.topLeft() + QPointF(HANDLE_MARGIN, iconMargin);
    QPointF step = QPointF(0, m_iconSize + iconMargin);
    QPointF separator = step + QPointF(0, iconMargin);
    //end duplicate code

    QPointF shiftC;
    QPointF shiftD;
    QPointF shiftR;
    QPointF shiftM;

    switch(m_pressedButton)
    {
    case ConfigureButton:
        shiftC = QPointF(2, 2);
        break;
    case RemoveButton:
        shiftD = QPointF(2, 2);
        break;
    case RotateButton:
        shiftR = QPointF(2, 2);
        break;
    case ResizeButton:
        shiftM = QPointF(2, 2);
        break;
    default:
        break;
    }

    QRectF sourceIconRect(QPointF(0, m_decorationRect.height() + 1), iconSize);
    if (m_buttonsOnRight) {
        sourceIconRect.moveLeft(
            pixmapSize.width() - m_iconSize - m_background->marginSize(Plasma::LeftMargin));
    } else {
        sourceIconRect.moveLeft(m_background->marginSize(Plasma::RightMargin));
    }

    //resize
    painter->drawPixmap(QRectF(basePoint + shiftM, iconSize), *m_backgroundBuffer, sourceIconRect);
    basePoint += step;

    //rotate
    sourceIconRect.translate(0, m_iconSize);
    painter->drawPixmap(QRectF(basePoint + shiftR, iconSize), *m_backgroundBuffer, sourceIconRect);

    //configure
//     if (m_group && m_group->hasConfigurationInterface()) {
//         basePoint += step;
//         sourceIconRect.translate(0, m_iconSize);
//         painter->drawPixmap(
//             QRectF(basePoint + shiftC, iconSize), *m_backgroundBuffer, sourceIconRect);
//     }

    //close
    basePoint = m_rect.bottomLeft() + QPointF(HANDLE_MARGIN, 0) - step;
    sourceIconRect.translate(0, m_iconSize);
    painter->drawPixmap(QRectF(basePoint + shiftD, iconSize), *m_backgroundBuffer, sourceIconRect);
}

void GroupHandle::emitDisappear()
{
    emit disappearDone(this);
}

GroupHandle::ButtonType GroupHandle::mapToButton(const QPointF &point) const
{
    int iconMargin = m_iconSize / 2;
    //XXX this code is duplicated in the prev. function
    QPointF basePoint = m_rect.topLeft() + QPointF(HANDLE_MARGIN, iconMargin);
    QPointF step = QPointF(0, m_iconSize + iconMargin);
    QPointF separator = step + QPointF(0, iconMargin);
   //end duplicate code

    QRectF activeArea = QRectF(basePoint, QSizeF(m_iconSize, m_iconSize));

    if (activeArea.contains(point)) {
        return ResizeButton;
    }
    activeArea.translate(step);

    if (activeArea.contains(point)) {
        return RotateButton;
    }

//     if (m_group && m_group->hasConfigurationInterface()) {
//         activeArea.translate(step);
//         if (activeArea.contains(point)) {
//             return ConfigureButton;
//         }
//     }

    activeArea.moveTop(m_rect.bottom() - activeArea.height() - iconMargin);
    if (activeArea.contains(point)) {
        return RemoveButton;
    }

    return MoveButton;
    //return m_group->mapToParent(m_group->shape()).contains(point) ? NoButton : MoveButton;
}

void GroupHandle::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    //containment recently switched?
    if (!m_group) {
        QGraphicsItem::mousePressEvent(event);
        return;
    }

    if (m_pendingFade) {
        //m_pendingFade = false;
        return;
    }

    if (event->button() == Qt::LeftButton) {
        m_pressedButton = mapToButton(event->pos());
        //kDebug() << "button pressed:" << m_pressedButton;
        if (m_pressedButton != NoButton) {
//             m_group->raise();
            m_zValue = m_group->zValue();
            setZValue(m_zValue);
        }

        if (m_pressedButton == ResizeButton || m_pressedButton == RotateButton) {
            m_originalGeom = m_group->geometry();
            m_origGroupCenter = m_originalGeom.center();
            m_origGroupSize = QPointF(m_group->size().width(), m_group->size().height());

            // resize
            if (m_buttonsOnRight) {
                m_resizeStaticPoint = mapToScene(m_group->geometry().bottomLeft());
            } else {
                m_resizeStaticPoint = mapToScene(m_group->geometry().bottomRight());
            }
            m_resizeGrabPoint = event->scenePos();

            // rotate
            m_rotateAngleOffset = m_angle - _k_pointAngle(event->scenePos() - m_origGroupCenter);
        }

        event->accept();

        //set mousePos to the position in the applet, in screencoords, so it becomes easy
        //to reposition the toplevel view to the correct position.
        if (m_currentView && m_group) {
            QPoint localpos = m_currentView->mapFromScene(m_group->scenePos());
            m_mousePos = event->screenPos() - m_currentView->mapToGlobal(localpos);
        }
        return;
    }

    QGraphicsItem::mousePressEvent(event);
}

bool GroupHandle::leaveCurrentView(const QPoint &pos) const
{
    foreach (QWidget *widget, QApplication::topLevelWidgets()) {
        if (widget->geometry().contains(pos)) {
            //is this widget a plasma view, a different view then our current one,
            //AND not a dashboardview?
            Plasma::View *v = qobject_cast<Plasma::View *>(widget);
            if (v && v != m_currentView && v->containment() != m_containment) {
                return true;
            }
        }
    }

    return false;
}

void GroupHandle::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    //kDebug() << "button pressed:" << m_pressedButton << ", fade pending?" << m_pendingFade;

    if (m_pendingFade) {
        startFading(FadeOut, m_entryPos);
        m_pendingFade = false;
    }

    ButtonType releasedAtButton = mapToButton(event->pos());

    if (m_group && event->button() == Qt::LeftButton) {
        switch (m_pressedButton) {
        case ConfigureButton:
            //FIXME: Remove this call once the configuration management change was done
            if (m_pressedButton == releasedAtButton) {
//                 m_group->showConfigurationInterface();
            }
            break;
        case RemoveButton:
            if (m_pressedButton == releasedAtButton) {
                forceDisappear();
                m_group->destroy();
                return;
            }
            break;
        case MoveButton:
        {
                // test for containment change
                //kDebug() << "testing for containment change, sceneBoundingRect = "
                //         << m_containment->sceneBoundingRect();
                if (!m_containment->sceneBoundingRect().contains(m_group->scenePos())) {
                    // see which containment it belongs to
                    Plasma::Corona *corona = qobject_cast<Plasma::Corona*>(scene());
                    if (corona) {
                        foreach (Plasma::Containment *containment, corona->containments()) {
                            QPointF pos;
                            QGraphicsView *v = containment->view();
                            if (v) {
                                pos = v->mapToScene(v->mapFromGlobal(event->screenPos() - m_mousePos));
                                GroupingContainment *c = qobject_cast<GroupingContainment *>(containment);
                                if (c && c->sceneBoundingRect().contains(pos)) {
                                    //kDebug() << "new containment = " << containments[i];
                                    //kDebug() << "rect = " << containments[i]->sceneBoundingRect();
                                    // add the applet to the new containment and take it from the old one
                                    //kDebug() << "moving to other containment with position" << pos;;
                                    switchContainment(c, pos);
                                    break;
                                }
                            }
                        }
                    }
                }
            break;
        }
        default:
            break;
        }
    }

    m_pressedButton = NoButton;
    update();
}

qreal _k_distanceForPoint(QPointF point)
{
    return std::sqrt(point.x() * point.x() + point.y() * point.y());
}

qreal _k_pointAngle(QPointF in)
{
    qreal r = sqrt(in.x()*in.x() + in.y()*in.y());
    qreal cosine = in.x()/r;
    qreal sine = in.y()/r;

    if (sine >= 0) {
        return acos(cosine);
    } else {
        return -acos(cosine);
    }
}

QPointF _k_rotatePoint(QPointF in, qreal rotateAngle)
{
    qreal r = sqrt(in.x()*in.x() + in.y()*in.y());
    qreal cosine = in.x()/r;
    qreal sine = in.y()/r;

    qreal angle;
    if (sine>=0) {
        angle = acos(cosine);
    } else {
        angle = -acos(cosine);
    }

    qreal newAngle = angle + rotateAngle;
    return QPointF(r*cos(newAngle), r*sin(newAngle));
}

void GroupHandle::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    static const qreal snapAngle = M_PI_2 /* $i 3.14159 / 2.0 */;

    if (!m_group) {
        QGraphicsItem::mouseMoveEvent(event);
        return;
    }

    //Track how much the mouse has moved.
    QPointF deltaScene  = event->scenePos() - event->lastScenePos();

    if (m_pressedButton == MoveButton) {
        if (leaveCurrentView(event->screenPos())) {
            Plasma::View *v = Plasma::View::topLevelViewAt(event->screenPos());
            if (v && v != m_currentView) {
                GroupingContainment *c = qobject_cast<GroupingContainment *>(v->containment());
                if (c) {
                    QPoint pos = v->mapFromGlobal(event->screenPos());
                    //we actually have been dropped on another containment, so
                    //move there: we have a screenpos, we need a scenepos
                    //FIXME how reliable is this transform?
                    switchContainment(c, v->mapToScene(pos));
                }
            }
        }

        if (m_group) {
            QPointF mappedPoint = transform().map(QPointF(deltaScene.x(), deltaScene.y()));
            m_group->moveBy(mappedPoint.x(), mappedPoint.y());
        }
    } else if (m_pressedButton == ResizeButton || m_pressedButton == RotateButton) {
        QPointF cursorPoint = event->scenePos();

        // the code below will adjust these based on the type of operation
        QPointF newSize;
        QPointF newCenter;
        qreal newAngle;

        // get size limits
        QSizeF min = m_group->minimumSize();
        QSizeF max = m_group->maximumSize();

        if (min.isEmpty()) {
            min = m_group->effectiveSizeHint(Qt::MinimumSize);
        }

        if (max.isEmpty()) {
            max = m_group->effectiveSizeHint(Qt::MaximumSize);
        }

        // If the applet doesn't have a minimum size, calculate based on a
        // minimum content area size of 16x16 (KIconLoader::SizeSmall)
        if (min.isEmpty()) {
            min = m_group->boundingRect().size() - m_group->contentsRect().size();
            min = QSizeF(KIconLoader::SizeSmall, KIconLoader::SizeSmall);
        }

        if (m_pressedButton == RotateButton) {
            newSize = m_origGroupSize;
            newCenter = m_origGroupCenter;

            QPointF centerRelativePoint = cursorPoint - m_origGroupCenter;
            if (_k_distanceForPoint(centerRelativePoint) < 10) {
                newAngle = m_angle;
            } else {
                qreal cursorAngle = _k_pointAngle(centerRelativePoint);
                newAngle = m_rotateAngleOffset + cursorAngle;
                if (fabs(remainder(newAngle, snapAngle)) < 0.15) {
                    newAngle = newAngle - remainder(newAngle, snapAngle);
                }
            }
        } else {
            // un-rotate screen points so we can read differences of coordinates
            QPointF rStaticPoint = _k_rotatePoint(m_resizeStaticPoint, -m_angle);
            QPointF rCursorPoint = _k_rotatePoint(cursorPoint, -m_angle);
            QPointF rGrabPoint = _k_rotatePoint(m_resizeGrabPoint, -m_angle);

            if (m_buttonsOnRight) {
                newSize = m_origGroupSize + QPointF(rCursorPoint.x() - rGrabPoint.x(), rGrabPoint.y() - rCursorPoint.y());
            } else {
                newSize = m_origGroupSize + QPointF(rGrabPoint.x() - rCursorPoint.x(), rGrabPoint.y() - rCursorPoint.y());
            }

            newSize.rx() = qMin(max.width(), qMax(min.width(), newSize.x()));
            newSize.ry() = qMin(max.height(), qMax(min.height(), newSize.y()));

            if (m_buttonsOnRight) {
                newCenter = (QPointF(m_originalGeom.size().width(), m_originalGeom.size().height())/2 - newSize/2);
                newCenter = QPointF(0, newCenter.y()*2);
            } else {
                newCenter = (QPointF(m_originalGeom.size().width(), m_originalGeom.size().height())/2 - newSize/2)*2;
            }

            newAngle = m_angle;
        }

        if (m_pressedButton == ResizeButton) {
            // set applet size
            //kDebug() << newCenter << m_originalGeom.topLeft() << newSize;
            QPointF newPos = m_originalGeom.topLeft() + _k_rotatePoint(newCenter, m_angle);
            m_group->setPos(newPos);
           // m_group->moveBy(newCenter.x(), newCenter.y());
            m_group->resize(newSize.x(), newSize.y());
        } else {
            // set applet handle rotation - rotate around center of applet
            QRectF groupGeomLocal = m_originalGeom;
            QTransform at;
            at.translate(groupGeomLocal.width()/2, groupGeomLocal.height()/2);
            at.rotateRadians(newAngle);
            at.translate(-groupGeomLocal.width()/2, -groupGeomLocal.height()/2);
            m_group->setTransform(at);
        }

        m_angle = newAngle;
    } else {
        QGraphicsItem::mouseMoveEvent(event);
    }
}

//pos relative to scene
void GroupHandle::switchContainment(GroupingContainment *containment, const QPointF &pos)
{
    m_containment = containment;
    AbstractGroup *group = m_group;
    m_group = 0; //make sure we don't try to act on the applet again
    group->removeSceneEventFilter(this);
    forceDisappear(); //takes care of event filter and killing handle
    group->disconnect(this); //make sure the applet doesn't tell us to do anything
    //applet->setZValue(m_zValue);
    containment->addGroup(group, containment->mapFromScene(pos));
    update();
}

void GroupHandle::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event);

    //if a disappear was scheduled stop the timer
    if (m_leaveTimer->isActive()) {
        m_leaveTimer->stop();
    }
    // if we're already fading out, fade back in
    else if (m_animId != 0 && m_anim == FadeOut) {
        startFading(FadeIn, m_entryPos, true);
    }
}

void GroupHandle::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    hoverEnterEvent(event);
}

void GroupHandle::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event);

    foreach (QWidget *widget, QApplication::topLevelWidgets()) {
      QMenu *menu = qobject_cast<QMenu*>(widget);
      if (menu && menu->isVisible()) {
          connect(menu, SIGNAL(aboutToHide()), this, SLOT(leaveTimeout()));
          return;
      }
    }


    // if we haven't even showed up yet, remove the handle
    if (m_hoverTimer->isActive()) {
        m_hoverTimer->stop();
        QTimer::singleShot(0, this, SLOT(emitDisappear()));
    } else if (m_pressedButton != NoButton) {
        m_pendingFade = true;
    } else {
        //wait a moment to hide the handle in order to recheck the mouse position
        m_leaveTimer->start();
    }
}

bool GroupHandle::sceneEventFilter(QGraphicsItem *watched, QEvent *event)
{
    if (watched == m_group && event->type() == QEvent::GraphicsSceneHoverLeave) {
        hoverLeaveEvent(static_cast<QGraphicsSceneHoverEvent *>(event));
    }

    return false;
}

void GroupHandle::fadeAnimation(qreal progress)
{
    //qreal endOpacity = (m_anim == FadeIn) ? 1.0 : -1.0;
    if (m_anim == FadeIn) {
        m_opacity = progress;
    } else {
        m_opacity = 1 - progress;
    }

    //kDebug() << "progress" << progress << "m_opacity" << m_opacity << m_anim << "(" << FadeIn << ")";
    if (qFuzzyCompare(progress, qreal(1.0))) {
        m_animId = 0;
        delete m_backgroundBuffer;
        m_backgroundBuffer = 0;
    }

    update();
}

void GroupHandle::hoverTimeout()
{
    startFading(FadeIn, m_entryPos);
}

void GroupHandle::leaveTimeout()
{
    if (!isUnderMouse()) {
        startFading(FadeOut, m_entryPos);
    }
}

void GroupHandle::groupDestroyed()
{
    m_group = 0;
}

void GroupHandle::groupResized()
{
    prepareGeometryChange();
    calculateSize();
    update();
}

void GroupHandle::setHoverPos(const QPointF &hoverPos)
{
    m_entryPos = hoverPos;
}

void GroupHandle::startFading(FadeType anim, const QPointF &hoverPos, bool preserveSide)
{
    if (m_animId != 0) {
        Plasma::Animator::self()->stopCustomAnimation(m_animId);
    }

    m_entryPos = hoverPos;
    qreal time = 100;

    if (!m_group) {
        m_anim = FadeOut;
        fadeAnimation(1.0);
        return;
    }

    if (anim == FadeIn) {
        //kDebug() << m_entryPos.x() << m_group->pos().x();
        prepareGeometryChange();
        bool wasOnRight = m_buttonsOnRight;
        if (!preserveSide) {
            m_buttonsOnRight = m_entryPos.x() > (m_group->size().width() / 2);
        }
        calculateSize();
        QPolygonF region = m_group->mapToParent(m_rect).intersected(m_group->parentWidget()->boundingRect());
        //kDebug() << region << m_rect << mapToParent(m_rect) << containmnet->boundingRect();
        if (region != m_group->mapToParent(m_rect)) {
            // switch sides
            //kDebug() << "switch sides";
            m_buttonsOnRight = !m_buttonsOnRight;
            calculateSize();
            QPolygonF region2 = m_group->mapToParent(m_rect).intersected(m_group->parentWidget()->boundingRect());
            if (region2 != mapToParent(m_rect)) {
                // ok, both sides failed to be perfect... which one is more perfect?
                QRectF f1 = region.boundingRect();
                QRectF f2 = region2.boundingRect();
                //kDebug() << "still not a perfect world"
                //         << f2.width() << f2.height() << f1.width() << f1.height();
                if ((f2.width() * f2.height()) < (f1.width() * f1.height())) {
                    //kDebug() << "we did better the first time";
                    m_buttonsOnRight = !m_buttonsOnRight;
                    calculateSize();
                }
            }
        }

        if (wasOnRight != m_buttonsOnRight &&
            m_anim == FadeIn &&
            anim == FadeIn &&
            m_opacity <= 1) {
            m_opacity = 0.0;
        }

        time *= 1.0 - m_opacity;
    } else {
        time *= m_opacity;
    }

    m_anim = anim;
    //kDebug() << "animating for " << time << "ms";
    m_animId = Plasma::Animator::self()->customAnimation(80 * (time / 1000.0), (int)time,
                                                 Plasma::Animator::EaseInCurve, this, "fadeAnimation");
}

void GroupHandle::forceDisappear()
{
    setAcceptsHoverEvents(false);
    startFading(FadeOut, m_entryPos);
}

int GroupHandle::minimumHeight()
{
    int iconMargin = m_iconSize / 2;
    int requiredHeight =  iconMargin  + //first margin
                          (m_iconSize + iconMargin) * 4 + //XXX remember to update this if the number of buttons changes
                          iconMargin ;  //blank space before the close button

//     if (m_group && m_group->hasConfigurationInterface()) {
//         requiredHeight += (m_iconSize + iconMargin);
//     }

    return requiredHeight;
}

void GroupHandle::calculateSize()
{
    KIconLoader *iconLoader = KIconLoader::global();
    //m_iconSize = iconLoader->currentSize(KIconLoader::Small); //does not work with double sized icon
    m_iconSize = iconLoader->loadIcon("transform-scale", KIconLoader::Small).width(); //workaround

    int handleHeight = qMax(minimumHeight(), int(m_group->contentsRect().height() * 0.8));
    int handleWidth = m_iconSize + 2 * HANDLE_MARGIN;
    int top =
        m_group->contentsRect().top() + (m_group->contentsRect().height() - handleHeight) / 2.0;

    qreal marginLeft, marginTop, marginRight, marginBottom;
    m_background->getMargins(marginLeft, marginTop, marginRight, marginBottom);

    if (m_buttonsOnRight) {
        //put the rect on the right of the applet
        m_rect = QRectF(m_group->size().width(), top, handleWidth, handleHeight);
    } else {
        //put the rect on the left of the applet
        m_rect = QRectF(-handleWidth, top, handleWidth, handleHeight);
    }

    if (m_group->contentsRect().height() > qreal(minimumHeight()) * 1.25) {
        int addedMargin = marginLeft / 2;

        // now we check to see if the shape is smaller than the contents,
        // and that the shape is not just the bounding rect; in those cases
        // we have a shaped guy and we draw a full panel;
        // TODO: allow applets to mark when they have translucent areas and
        //       should therefore skip this test?
        if (!m_group->shape().contains(m_group->contentsRect())) {
            QPainterPath p;
            p.addRect(m_group->boundingRect());
            if (m_group->shape() != p) {
                addedMargin = m_group->contentsRect().width() / 2;
            }
        }

        if (m_buttonsOnRight) {
            marginLeft += addedMargin;
        } else {
            marginRight += addedMargin;
        }
    }

    //m_rect = m_group->mapToParent(m_rect).boundingRect();
    m_decorationRect = m_rect.adjusted(-marginLeft, -marginTop, marginRight, marginBottom);
    m_totalRect = m_decorationRect.united(m_group->boundingRect());
}

#include "grouphandle.moc"
