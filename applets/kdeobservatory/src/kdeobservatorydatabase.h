/*************************************************************************
 * Copyright 2009 Sandro Andrade sandroandrade@kde.org                   *
 *                                                                       *
 * This program is free software; you can redistribute it and/or         *
 * modify it under the terms of the GNU General Public License as        *
 * published by the Free Software Foundation; either version 2 of        *
 * the License or (at your option) version 3 or any later version        *
 * accepted by the membership of KDE e.V. (or its successor approved     *
 * by the membership of KDE e.V.), which shall act as a proxy            *
 * defined in Section 14 of version 3 of the license.                    *
 *                                                                       *
 * This program is distributed in the hope that it will be useful,       *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 * GNU General Public License for more details.                          *
 *                                                                       *
 * You should have received a copy of the GNU General Public License     *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 * ***********************************************************************/

#ifndef KDEOBSERVATORYDATABASE_HEADER
#define KDEOBSERVATORYDATABASE_HEADER

#include <QPair>
#include <QSqlQuery>
#include <QMultiMap>
#include <QSqlDatabase>

class KdeObservatoryDatabase
{
public:
    virtual ~KdeObservatoryDatabase();
    static KdeObservatoryDatabase *self();

    void beginTransaction();
    void commitTransaction();
    void addCommit(const QString &date, const QString &subject, const QString &developer);
    void truncateCommits();
    void deleteOldCommits(const QString &date);
    int commitsByProject(const QString &prefix);
    QMultiMap<int, QString> developersByProject(const QString &prefix);
    QList< QPair<QString, int> > commitHistory(const QString &prefix);

    void addKrazyError(const QString &project, const QString &fileType, const QString &testName, const QString &fileName, const QString &error);
    QMap<QString, QMultiMap<int, QString> > krazyErrorsByProject(const QString &project);
    QStringList krazyFilesByProjectTypeAndTest(const QString &project, const QString &fileType, const QString &testName);
    void truncateKrazyErrors();

private:
    KdeObservatoryDatabase();
    void init();

    static KdeObservatoryDatabase *m_kdeObservatoryDatabase;
    QSqlDatabase m_db;
    QSqlQuery m_query;
};

#endif