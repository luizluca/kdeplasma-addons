/*
 *   Copyright (C) 2007 Ivan Cukic <ivan.cukic(at)kde.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License version 2,
 *   or (at your option) any later version, as published by the Free
 *   Software Foundation
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "Runner.h"
#include <KRun>
#include <KIcon>
#include <KLocalizedString>
#include <KApplication>
#include <KStandardDirs>
#include "logger/Logger.h"
#include <QDebug>

#include <plasma/abstractrunner.h>
#include "FavoriteApplications.h"
#define SLEEP 200

namespace Models {

Runner::Runner(bool limitRunners, QString search)
    : m_searchString(search), valid(false)
{
    m_runnerManager = new Plasma::RunnerManager(this);

    if (limitRunners) {
        QStringList allowed;
        allowed
            << "places"
            << "shell"
            << "services"
            << "bookmarks"
            << "recentdocuments"
            << "locations";
        m_runnerManager->setAllowedRunners(allowed);
    }

    connect(
        m_runnerManager, SIGNAL(matchesChanged(const QList<Plasma::QueryMatch>&)),
        this, SLOT(setQueryMatches(const QList<Plasma::QueryMatch>&))
    );
    setSearchString(QString());
}

Runner::Runner(QStringList allowedRunners, QString search)
    : m_searchString(search), valid(false)
{
    m_runnerManager = new Plasma::RunnerManager(this);
    m_runnerManager->setAllowedRunners(allowedRunners);

    connect(
        m_runnerManager, SIGNAL(matchesChanged(const QList<Plasma::QueryMatch>&)),
        this, SLOT(setQueryMatches(const QList<Plasma::QueryMatch>&))
    );
    setSearchString(QString());
}

Runner::~Runner()
{
}

QString Runner::searchString() const
{
    return m_searchString;
}

QString Runner::runnerName() const
{
    return m_runnerName;
}

void Runner::setRunnerName(const QString & name)
{
    m_runnerName = name;
}

void Runner::setSearchString(const QString & search)
{
    m_searchString = search.trimmed();
    clear();

    if (m_searchString.isEmpty()) {
        add(
            i18n("Search string is empty"),
            i18n("Enter something to search for"),
            KIcon("help-hint"),
            QVariant()
        );
        valid = false;
    } else {
        add(
            i18n("Searching..."),
            i18n("Some searches can take longer to complete"),
            KIcon("help-hint"),
            QVariant()
        );
        valid = false;
    }

    m_timer.start(SLEEP, this);
}

void Runner::timerEvent(QTimerEvent * event)
{
    BaseModel::timerEvent(event);
    if (event->timerId() != m_timer.timerId()) {
        return;
    }

    m_timer.stop();

    if (!m_searchString.isEmpty()) {
        m_runnerManager->reset();

        if (m_runnerName.isEmpty()) {
            m_runnerManager->launchQuery(m_searchString);
        } else {
            m_runnerManager->launchQuery(m_searchString, m_runnerName);
        }

        // foreach (Plasma::AbstractRunner * runner, m_runnerManager->runners()) {
        //     qDebug() << "Runner: " << runner->id();
        // }
    }
}

// Code taken from KRunner Runner::setQueryMatches
void Runner::setQueryMatches(const QList< Plasma::QueryMatch > & m)
{
    qDebug() << "Runner::setQueryMatches" << m.size();
    setEmitInhibited(true);
    clear();

    if (m.count() == 0) {
        add(
            i18n("No matches found"),
            i18n("No matches found for current search"),
            KIcon("help-hint"),
            QVariant()
        );
        valid = false;
    } else {
        QList < Plasma::QueryMatch > matches = m;
        qSort(matches.begin(), matches.end());

        while (matches.size()) {
            Plasma::QueryMatch match = matches.takeLast();
            QStringList data;
            data << match.id();
            data << match.runner()->id();
            data << match.data().toString();

            add(
                match.text(),
                match.subtext(),
                match.icon(),
                data
            );
        }
        valid = true;
    }
    setEmitInhibited(false);
    emit updated();
}

void Runner::load()
{
}

void Runner::activate(int index)
{
    if (!valid) return;
    QString data = itemAt(index).data.value< QStringList >().at(0);
    Logger::self()->log("run-model", data);
    m_runnerManager->run(data);
    m_runnerManager->reset();
    changeLancelotSearchString(QString());
    hideLancelotWindow();
}

bool Runner::hasContextActions(int index) const
{
    if (!valid) return false;

    if (itemAt(index).data.value< QStringList >().at(1) == "services") {
        return true;
    }

    QString id = itemAt(index).data.value< QStringList >().at(0);
    foreach (const Plasma::QueryMatch &match, m_runnerManager->matches()) {
        if (match.id() == id) {
            if (m_runnerManager->actionsForMatch(match).size() > 0) {
                return true;
            }
        }
    }

    return false;
}

void Runner::setContextActions(int index, Lancelot::PopupMenu * menu)
{
    if (!valid) return;

    if (itemAt(index).data.value< QStringList >().at(1) == "services") {
        menu->addAction(KIcon("list-add"), i18n("Add to Favorites"))
            ->setData(QVariant(0));
    }

    QString id = itemAt(index).data.value< QStringList >().at(0);
    foreach (const Plasma::QueryMatch &match, m_runnerManager->matches()) {
        if (match.id() == id) {
            foreach (QAction * action, m_runnerManager->actionsForMatch(match)) {
                menu->addAction(action->icon(), action->text());
            }
        }
    }
}

void Runner::contextActivate(int index, QAction * context)
{
    if (!valid || !context) return;

    if (context->data().toInt() == 0) {
        KService::Ptr service = KService::serviceByStorageId(
                itemAt(index).data.value< QStringList >().at(2));
        if (service) {
            FavoriteApplications::self()
                ->addFavorite(service->entryPath());
        }
    }
}

QMimeData * Runner::mimeData(int index) const
{
    if (!valid) return NULL;

    if (itemAt(index).data.value< QStringList >().at(1) == "services") {
        KService::Ptr service = KService::serviceByStorageId(
                itemAt(index).data.value< QStringList >().at(2));
        return BaseModel::mimeForService(service);
    }

    return NULL;
}

void Runner::setDropActions(int index,
            Qt::DropActions & actions, Qt::DropAction & defaultAction)
{
    Q_UNUSED(index);
    actions = Qt::CopyAction;
    defaultAction = Qt::CopyAction;
}

} // namespace Models