/*
 *   Copyright (C) 2009, 2010 Ivan Cukic <ivan.cukic(at)kde.org>
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

#include "PartsMergedModel.h"

#include <QFile>
#include <QTextStream>

#include <KIcon>
#include <KLocalizedString>
#include <KDebug>

#include <Lancelot/Models/BaseModel>
#include <Lancelot/Models/Devices>
#include <Lancelot/Models/Places>
#include <Lancelot/Models/SystemServices>
#include <Lancelot/Models/RecentDocuments>
#include <Lancelot/Models/OpenDocuments>
#include <Lancelot/Models/NewDocuments>
#include <Lancelot/Models/FolderModel>
#include <Lancelot/Models/FavoriteApplications>
#include <Lancelot/Models/Applications>
#include <Lancelot/Models/Runner>
#include <Lancelot/Models/ContactsKopete>
#include <Lancelot/Models/MessagesKmail>
#include <Lancelot/Models/SystemActions>
#include <Lancelot/Models/Serializator>

namespace Models {

PartsMergedModel::PartsMergedModel()
    : Lancelot::Models::BaseMergedModel()
{
}

PartsMergedModel::~PartsMergedModel()
{
    clear();
    // qDeleteAll(m_models);
}

bool PartsMergedModel::hasModelContextActions(int index) const
{
    Q_UNUSED(index);
    return true;
}

void PartsMergedModel::setModelContextActions(int index, Lancelot::PopupMenu * menu)
{
    Q_UNUSED(index);
    menu->addAction(KIcon("list-remove"), i18n("Remove this"))
        ->setData(QVariant(0));
}

void PartsMergedModel::modelContextActivate(int index, QAction * context)
{
    if (!context) {
        return;
    }

    if (context->data().toInt() == 0) {
        emit removeModelRequested(index);
    }
}

void PartsMergedModel::setModelDropActions(int index, Qt::DropActions & actions,
        Qt::DropAction & defaultAction)
{
    Q_UNUSED(index);
    if (modelCount() <= 1) {
        actions = Qt::CopyAction;
        defaultAction = Qt::CopyAction;
    } else {
        actions = Qt::CopyAction | Qt::MoveAction;
        defaultAction = Qt::MoveAction;
    }
}

void PartsMergedModel::modelDataDropped(int index, Qt::DropAction action)
{
    if (action == Qt::MoveAction) {
        emit removeModelRequested(index);
    }
}

bool PartsMergedModel::dataDropAvailable(int where, const QMimeData * mimeData)
{
    kDebug() << mimeData->formats();

    if (mimeData->formats().contains("text/x-lancelotpart") ||
        mimeData->formats().contains("inode/directory")) {
        return true;
    }

    if (mimeData->formats().contains("text/uri-list")) {
        // TODO: Check whether urls are for directories
        return true;
    }

    return BaseMergedModel::dataDropAvailable(where, mimeData);
}

void PartsMergedModel::dataDropped(int where, const QMimeData * mimeData)
{
    if (mimeData->formats().contains("text/x-lancelotpart") ||
            mimeData->formats().contains("inode/directory") ||
            mimeData->formats().contains("text/uri-list")
            ) {
        append(mimeData);
    }

    BaseMergedModel::dataDropped(where, mimeData);
}

bool PartsMergedModel::append(const QString & data)
{
    return load(data);
}

bool PartsMergedModel::append(const QMimeData * mimeData)
{
    if (mimeData->hasFormat("text/x-lancelotpart")) {
        QString data = mimeData->data("text/x-lancelotpart");
        load(data);
        return true;
    }

    if (!mimeData->hasFormat("text/uri-list")) {
        return false;
    }

    QString file = mimeData->data("text/uri-list");
    kDebug() << file;

    KMimeType::Ptr mimeptr = KMimeType::findByUrl(KUrl(file));
    if (!mimeptr) {
        return false;
    }
    QString mime = mimeptr->name();

    if (mime != "text/x-lancelotpart" && mime != "inode/directory") {
        return false;
    }

    if (mime == "inode/directory") {
        return loadDirectory(file);
    } else {
        return loadFromFile(file);
    }
}

bool PartsMergedModel::append(const QString & path, const KFileItem & fileItem)
{
    if (fileItem.mimetype() == "inode/directory") {
        return loadDirectory(path);
    } else {
        return loadFromFile(path);
    }
}

void PartsMergedModel::remove(int index)
{
    Lancelot::ActionListModel * model = modelAt(index);
    removeModel(index);
    if (m_models.contains(model)) {
        delete model;
        m_models.removeAll(model);
    }

    QStringList configs = m_data.split('\n');
    configs.removeAt(index);
    m_data = configs.join("\n");

    emit modelContentsUpdated();
}

void PartsMergedModel::clear()
{
    while (modelCount()) {
        removeModel(0);
    }

    qDeleteAll(m_models);
    m_models.clear();

    m_data.clear();
}

bool PartsMergedModel::loadFromFile(const QString & url)
{
    bool loaded = false;

    QFile file(QUrl(url).toLocalFile());
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (load(line)) {
                loaded = true;
            }
        }
    }

    return loaded;
}

bool PartsMergedModel::loadDirectory(const QString & url)
{
    QMap < QString, QString > data;
    data["version"]     = "1.0";
    data["type"]        = "list";
    data["model"]       = "Folder " + url;
    return load(Lancelot::Models::Serializator::serialize(data));
}

bool PartsMergedModel::load(const QString & input)
{
    QMap < QString, QString > data = Lancelot::Models::Serializator::deserialize(input);
    kDebug() << data;

    if (!data.contains("version")) {
        return false;
    }

    bool loaded = false;

    if (data["version"] <= "1.0") {
        if (data["type"] == "list") {
            QStringList modelDef = data["model"].split(' ');
            QString modelID = modelDef[0];
            QString modelExtraData;

            kDebug() << modelDef << modelID << modelExtraData;

            if (modelDef.size() != 1) {
                modelExtraData = modelDef[1];
            }
            Lancelot::ActionListModel * model = NULL;

            if (modelID == "Places") {
                addModel(modelID, QIcon(), i18n("Places"),
                        model = new Lancelot::Models::Places());
                m_models.append(model);

            } else if (modelID == "System") {
                addModel(modelID, QIcon(), i18n("System"),
                        model = new Lancelot::Models::SystemServices());
                m_models.append(model);

            } else if (modelID == "Devices/Removable") {
                addModel(modelID, QIcon(), i18n("Removable devices"),
                        model = new Lancelot::Models::Devices(Lancelot::Models::Devices::Removable));
                m_models.append(model);

            } else if (modelID == "Devices/Fixed") {
                addModel(modelID, QIcon(), i18n("Fixed devices"),
                        model = new Lancelot::Models::Devices(Lancelot::Models::Devices::Fixed));
                m_models.append(model);

            } else if (modelID == "NewDocuments") {
                addModel(modelID, QIcon(), i18n("New Documents"),
                        model = new Lancelot::Models::NewDocuments());
                m_models.append(model);

            } else if (modelID == "OpenDocuments") {
                addModel(modelID, QIcon(), i18n("Open Documents"),
                        model = new Lancelot::Models::OpenDocuments());
                m_models.append(model);

            } else if (modelID =="RecentDocuments") {
                addModel(modelID, QIcon(), i18n("Recent Documents"),
                        model = new Lancelot::Models::RecentDocuments());
                m_models.append(model);

            } else if (modelID =="Messages") {
                addModel(modelID, QIcon(), i18n("Unread messages"),
                        model = new Lancelot::Models::MessagesKmail());
                m_models.append(model);

            } else if (modelID =="Contacts") {
                addModel(modelID, QIcon(), i18n("Online contacts"),
                        model = new Lancelot::Models::ContactsKopete());
                m_models.append(model);

            } else if (modelID == "FavoriteApplications") {
                // We don't want to delete this one (singleton)
                addModel(modelID, QIcon(), i18n("Favorite Applications"),
                        model = Lancelot::Models::FavoriteApplications::self());

            } else if (modelID == "SystemActions") {
                // We don't want to delete this one (singleton)
                if (modelExtraData.isEmpty()) {
                    addModel(modelID, QIcon(), i18n("System"),
                            model = Lancelot::Models::SystemActions::self());
                } else {
                    model = Lancelot::Models::SystemActions::self()->action(modelExtraData, false);
                    if (!model) return false;
                    addModel(modelID, QIcon(), i18n("System"), model);
                }

            } else if (modelID == "Folder") {
                kDebug() << modelExtraData;
                if (modelExtraData.startsWith(QLatin1String("applications:/"))) {
                    modelExtraData.remove(0, 14);
                    addModel(modelExtraData,
                        QIcon(),
                        modelExtraData,
                        model = new Lancelot::Models::Applications(modelExtraData, QString(), QIcon(), true));
                } else {
                    addModel(modelExtraData,
                        QIcon(),
                        modelExtraData,
                        model = new Lancelot::Models::FolderModel(modelExtraData));
                }
                m_models.append(model);

            }
            loaded = (model != NULL);
        }
    }

    if (loaded) {
        if (!m_data.isEmpty()) {
            m_data += '\n';
        }
        m_data += input;
    }

    emit modelContentsUpdated();
    return loaded;
}

QString PartsMergedModel::serializedData() const
{
    return m_data;
}

} // namespace Models