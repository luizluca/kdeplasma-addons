/*************************************************************************
 * Copyright 2009-2010 Sandro Andrade sandroandrade@kde.org              *
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

#include "krazyreportview.h"

#include <QPen>

#include <KIcon>
#include <KGlobalSettings>

KrazyReportView::KrazyReportView(const QHash<QString, bool> &krazyReportViewProjects, const QMap<QString, KdeObservatory::Project> &projects, QGraphicsWidget *parent, Qt::WindowFlags wFlags)
: IViewProvider(parent, wFlags),
  m_krazyReportViewProjects(krazyReportViewProjects),
  m_projects(projects)
{
}

KrazyReportView::~KrazyReportView()
{
}

void KrazyReportView::createViews()
{
    deleteViews();
    // We don't know in advance how many Krazy report views will be created
    // so view creation is postponed to the updateViews function.
}

void KrazyReportView::updateViews(const Plasma::DataEngine::Data &data)
{
    QString project = data["project"].toString();
    if (project.isEmpty())
        return;

    KrazyReportMap krazyReportMap = data[project].value<KrazyReportMap>();

    foreach (const QString &fileType, krazyReportMap.keys())
        createView(i18n("Krazy Report") + " - " + project, i18n("Krazy Report") + " - " + project + " - " + fileType);
    
    KdeObservatory *kdeObservatory = dynamic_cast<KdeObservatory *>(m_parent->parentItem()->parentItem());

    KrazyReportMapIterator i1(krazyReportMap);

    // For each file type
    while (i1.hasNext())
    {
        i1.next();

        QString fileType = i1.key();
        const QMap< QString, FileTypeKrazyReportMap > &projectFileTypeKrazyReport = i1.value();

        QGraphicsWidget *container = containerForView(i18n("Krazy Report") + " - " + project + " - " + fileType);

        QGraphicsTextItem *fileTypeText = new QGraphicsTextItem(fileType, container);
        fileTypeText->setDefaultTextColor(QColor(0, 0, 0));
        fileTypeText->setFont(KGlobalSettings::smallestReadableFont());
        fileTypeText->setPos((qreal) ((container->rect().width())/2)-(fileTypeText->boundingRect().width()/2), (qreal) 0);

        int maxRank = 0;
        qreal height = container->geometry().height() - fileTypeText->boundingRect().height();
        qreal step = qMin(container->geometry().width() / projectFileTypeKrazyReport.size(), (qreal) 22);

        QMultiMap<int, QString> orderedTestMap;
        
        QMapIterator< QString, FileTypeKrazyReportMap > i2(projectFileTypeKrazyReport);
        int errors;
        while (i2.hasNext())
        {
            i2.next();
            errors = 0;
            foreach (const QStringList &list, i2.value())
                errors += list.count();
            orderedTestMap.insert(errors, i2.key());
        }
        
        int j = 0;
        QMapIterator<int, QString> i3(orderedTestMap);
        i3.toBack();
        
        // For each krazy test
        while (i3.hasPrevious())
        {
            i3.previous();
            QString testName = i3.value();
            int rank = i3.key();
            if (j == 0)
                maxRank = rank;

            qreal heightFactor = height/maxRank;
            qreal xItem = (j*step)+2;

            QGraphicsRectItem *testNameRect = new QGraphicsRectItem(0, 0, (qreal) step-4, (qreal) heightFactor*rank, container);
            testNameRect->setPos(xItem, container->geometry().height() - testNameRect->rect().height());
            testNameRect->setPen(QPen(QColor(0, 0, 0)));
            testNameRect->setBrush(QBrush(QColor::fromHsv(qrand() % 256, 255, 190), Qt::SolidPattern));
            QString toolTip = "<html><body><h5>" + testName + ' ' + QString::number(rank) + ' ' + i18np("error", "errors", rank) + "<ul>";

            const FileTypeKrazyReportMap &fileErrors = projectFileTypeKrazyReport[testName];
            QMapIterator<QString, QStringList > i4(fileErrors);
            while (i4.hasNext())
            {
                i4.next();
                foreach (const QString &error, i4.value())
                    toolTip += "<li>" + i4.key() + ": " + error + "</li>";
            }

            toolTip += "</ul></h5></body></html>";
            testNameRect->setToolTip(toolTip);

            testNameRect->setAcceptHoverEvents(true);
            testNameRect->installSceneEventFilter(kdeObservatory);

            j++;
        }
    }
}
