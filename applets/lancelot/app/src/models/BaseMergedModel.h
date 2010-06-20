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

#ifndef LANCELOTAPP_MODELS_BASEMERGEDMODEL_H
#define LANCELOTAPP_MODELS_BASEMERGEDMODEL_H

#include <lancelot/models/MergedActionListModel.h>

namespace Models {

class BaseMergedModel: public Lancelot::MergedActionListModel {
public:
    BaseMergedModel();
    ~BaseMergedModel();

    void addModel(const QString & id, QIcon icon,
            const QString & title, ActionListModel * model);
    void addModel(const QString & id, ActionListModel * model);
    L_Override QMimeData * modelMimeData(int index) const;
    L_Override void setModelDropActions(int index,
            Qt::DropActions & actions, Qt::DropAction & defaultAction);

private:
    QList < QString > m_modelIDs;
};

} // namespace Models

#endif // LANCELOTAPP_MODELS_BASEMERGEDMODEL_H