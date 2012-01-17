/****************************************************************************
**
** Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the QtLocation module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qdeclarativeresultmodelbase_p.h"

QDeclarativeResultModelBase::QDeclarativeResultModelBase(QObject *parent)
:   QDeclarativeSearchModelBase(parent), m_favoritesPlugin(0)
{
    QHash<int, QByteArray> roles = roleNames();
    roles.insert(DistanceRole, "distance");
    roles.insert(PlaceRole, "place");
    setRoleNames(roles);
}

QDeclarativeGeoServiceProvider* QDeclarativeResultModelBase::favoritesPlugin() const
{
    return m_favoritesPlugin;
}

void QDeclarativeResultModelBase::setFavoritesPlugin(QDeclarativeGeoServiceProvider *plugin)
{

    if (m_favoritesPlugin == plugin)
        return;

    m_favoritesPlugin = plugin;
    emit favoritesPluginChanged();
}

QVariantMap QDeclarativeResultModelBase::favoritesMatchParameters() const
{
    return m_matchParameters;
}

void QDeclarativeResultModelBase::setFavoritesMatchParameters(const QVariantMap &parameters)
{
    if (m_matchParameters == parameters)
        return;

    m_matchParameters = parameters;
    emit favoritesMatchParametersChanged();
}

void QDeclarativeResultModelBase::clearData()
{
    qDeleteAll(m_places);
    m_places.clear();
    m_results.clear();
}

int QDeclarativeResultModelBase::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);

    return m_results.count();
}

QVariant QDeclarativeResultModelBase::data(int index, const QString &role) const
{
    QModelIndex modelIndex = createIndex(index, 0);
    return data(modelIndex, roleNames().key(role.toLatin1()));
}

QVariant QDeclarativeResultModelBase::data(const QModelIndex &index, int role) const
{
    if (index.row() > m_results.count())
        return QVariant();

    const QPlaceSearchResult &result = m_results.at(index.row());

    switch (role) {
    case Qt::DisplayRole:
        if (result.type() == QPlaceSearchResult::PlaceResult)
            return m_places.at(index.row())->name();
        else
            return QVariant();
    case DistanceRole:
        return result.distance();
    case PlaceRole:
        return QVariant::fromValue(static_cast<QObject *>(m_places.at(index.row())));
    default:
        return QVariant();
    }
}

void QDeclarativeResultModelBase::queryFinished()
{
    if (!m_reply)
        return;
    QPlaceReply *reply = m_reply;
    m_reply = 0;
    if (reply->error() != QPlaceReply::NoError) {
        m_resultsBuffer.clear();
        updateLayout();
        setStatus(Error, reply->errorString());
        reply->deleteLater();
    }

    if (reply->type() == QPlaceReply::SearchReply) {
        QPlaceSearchReply *searchReply = qobject_cast<QPlaceSearchReply *>(reply);
        Q_ASSERT(searchReply);

        m_resultsBuffer = searchReply->results();
        reply->deleteLater();

        if (!m_favoritesPlugin) {
            updateLayout();
            setStatus(Ready);
        } else {
            QGeoServiceProvider *serviceProvider = m_favoritesPlugin->sharedGeoServiceProvider();
            if (!serviceProvider) {
                updateLayout();
                setStatus(Error, QLatin1String("Favorites plugin returns a null QGeoServiceProvider instance"));
            }

            QPlaceManager *favoritesManager = serviceProvider->placeManager();
            if (!favoritesManager) {
                updateLayout();
                setStatus(Error, QLatin1String("Favorites plugin returns a null QPlaceManager"));
            }

            QPlaceMatchRequest request;
            if (m_matchParameters.isEmpty()) {
                if (!m_plugin) {
                    reply->deleteLater();
                    setStatus(Error, QLatin1String("Plugin not assigned"));
                    return;
                }

                QVariantMap params;
                params.insert(QPlaceMatchRequest::AlternativeId, QString::fromLatin1("x_id_") + m_plugin->name());
                request.setParameters(params);
            } else {
                request.setParameters(m_matchParameters);
            }

            request.setResults(m_resultsBuffer);
            m_reply = favoritesManager->matchingPlaces(request);
            connect(m_reply, SIGNAL(finished()), this, SLOT(queryFinished()));
        }
    } else if (reply->type() == QPlaceReply::MatchReply){
        QPlaceMatchReply *matchReply = qobject_cast<QPlaceMatchReply *>(reply);
        Q_ASSERT(matchReply);
        updateLayout(matchReply->places());
        setStatus(Ready);
        reply->deleteLater();
    } else {
        setStatus(Error, QLatin1String("Unknown reply type"));
        reply->deleteLater();
    }
}


//Note: m_results buffer should be correctly populated before
//calling this function
void QDeclarativeResultModelBase::updateLayout(const QList<QPlace> &favoritePlaces)
{
    int oldRowCount = rowCount();

    beginResetModel();
    clearData();
    m_results = m_resultsBuffer;
    m_resultsBuffer.clear();

    for (int i=0 ;i < m_results.count(); ++i) {
        QDeclarativePlace *place = new QDeclarativePlace(m_results.at(i).place(),plugin(), this);
        m_places.append(place);

        if ((favoritePlaces.count() == m_results.count()) && favoritePlaces.at(i) != QPlace())
            m_places[i]->setFavorite(new QDeclarativePlace(favoritePlaces.at(i), m_favoritesPlugin, m_places[i]));
    }

    endResetModel();
    if (m_results.count() != oldRowCount)
        emit rowCountChanged();
}
