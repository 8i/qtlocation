/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/
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
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "jsondb.h"
#include "qgeoboundingcircle.h"
#include "qplacemanagerengine_jsondb.h"

#include <QtCore/QDebug>
#include <QtCore/QJsonArray>
#include <QtCore/QString>
#include <QtJsonDb/QJsonDbReadRequest>
#include <QtJsonDb/QJsonDbWriteRequest>
#include <QtLocation/QPlaceSearchRequest>
#include <QtLocation/QPlaceCategory>

const QLatin1String JsonDb::Uuid("_uuid");
const QLatin1String JsonDb::Type("_type");

const QLatin1String JsonDb::Name("displayName");
const QLatin1String JsonDb::PlaceType("com.nokia.mt.location.Place");
const QLatin1String JsonDb::CategoryUuids("categoryUuids"); //only categories that are directly assigned to a place
const QLatin1String JsonDb::AllCategoryUuids("allCategoryUuids"); //all categories that a place belongs to,
                                                         //it includes all ancestors of the assigned categories

const QLatin1String JsonDb::CategoryType("com.nokia.mt.location.PlaceCategory");
const QLatin1String JsonDb::Lineage("lineageUuids");  //includes all ancestor category ids and also the current category id
                                              //as the last element.   The first category is a top level category id.
const QLatin1String JsonDb::CategoryParentId("parentUuid");

//coord
const QLatin1String JsonDb::Coordinate("geo");
const QLatin1String JsonDb::Latitude("latitude");
const QLatin1String JsonDb::Longitude("longitude");

//address
const QLatin1String JsonDb::Address("address");
const QLatin1String JsonDb::Street("street");
const QLatin1String JsonDb::District("district");
const QLatin1String JsonDb::City("locality");
const QLatin1String JsonDb::County("county");
const QLatin1String JsonDb::State("region");
const QLatin1String JsonDb::Country("country");
const QLatin1String JsonDb::CountryCode("countryCode");
const QLatin1String JsonDb::PostalCode("postalCode");

const QLatin1String JsonDb::Location("location");

//contact details
const QLatin1String JsonDb::Phones("phones");
const QLatin1String JsonDb::Emails("emails");
const QLatin1String JsonDb::Websites("urls");

const QLatin1String JsonDb::PhoneSubType("subType");
const QLatin1String JsonDb::SubTypeFax("fax");
const QLatin1String JsonDb::SubTypeLandline("landline");

const QLatin1String JsonDb::Label("label");
const QLatin1String JsonDb::Value("value");
const QLatin1String JsonDb::Url("url");

const QLatin1String JsonDb::ExtendedAttributes("extendedAttributes");
const QLatin1String JsonDb::Text("text");

const QLatin1String JsonDb::Height("height");
const QLatin1String JsonDb::Width("width");

const QLatin1String JsonDb::Thumbnails("thumbnails");
const QLatin1String JsonDb::Small("small");
const QLatin1String JsonDb::Medium("medium");
const QLatin1String JsonDb::Large("large");
const QLatin1String JsonDb::Fullscreen("fullscreen");

const QLatin1String JsonDb::VisibilityScope("visibilityScope");
const QLatin1String JsonDb::DeviceVisibility("device");

const QLatin1String JsonDb::CreatedDateTime("createdDateTime");
const QLatin1String JsonDb::ModifiedDateTime("modifiedDateTime");

JsonDb::JsonDb()
    : m_connection(new QJsonDbConnection),
      m_placeWatcher(new QJsonDbWatcher(this)),
      m_categoryWatcher(new QJsonDbWatcher(this))
{
    m_connection->connectToServer();

    m_placeWatcher->setQuery(QString::fromLatin1("[?%1 = \"%2\"]").arg(JsonDb::Type).arg(JsonDb::PlaceType));
    connect(m_placeWatcher, SIGNAL(notificationsAvailable(int)),
            this, SLOT(processPlaceNotifications()));
    connect(m_placeWatcher, SIGNAL(error(QtJsonDb::QJsonDbWatcher::ErrorCode,QString)),
            this, SIGNAL(notificationsError(QtJsonDb::QJsonDbWatcher::ErrorCode,QString)));
    m_connection->addWatcher(m_placeWatcher);

    m_categoryWatcher->setQuery(QString::fromLatin1("[?%1 = \"%2\"]").arg(JsonDb::Type).arg(JsonDb::CategoryType));
    connect(m_categoryWatcher, SIGNAL(notificationsAvailable(int)),
            this, SLOT(processCategoryNotifications()));
    connect(m_categoryWatcher, SIGNAL(error(QtJsonDb::QJsonDbWatcher::ErrorCode,QString)),
            this, SIGNAL(notificationsError(QtJsonDb::QJsonDbWatcher::ErrorCode,QString)));
    m_connection->addWatcher(m_categoryWatcher);
}

JsonDb::~JsonDb()
{
    delete m_placeWatcher;
    delete m_categoryWatcher;
    delete m_connection;
}

QString JsonDb::convertToQueryString(const QPlaceSearchRequest &request)
{
    QString queryString;
    if (!request.searchTerm().isEmpty()) {
        queryString += QString::fromLatin1("[?%1=\"%2\"][?%3 =~ \"!.*%4.*!i\"]")
                        .arg(JsonDb::Type).arg(JsonDb::PlaceType).arg(JsonDb::Name).arg(request.searchTerm());
    }

    if (queryString.isEmpty())
        queryString = QString::fromLatin1("[?%1 = \"%2\"]").arg(JsonDb::Type).arg(JsonDb::PlaceType);

    if (request.relevanceHint() == QPlaceSearchRequest::LexicalPlaceNameHint)
        queryString += QString::fromLatin1("[/") + JsonDb::Name + QLatin1String("]");

    return queryString;
}

void JsonDb::addToJson(QJsonObject *jsonObj, const QPlaceCategory &category)
{
    if (!category.categoryId().isEmpty())
        jsonObj->insert(JsonDb::Uuid, category.categoryId());

    jsonObj->insert(JsonDb::Type, JsonDb::CategoryType);
    jsonObj->insert(JsonDb::Name, category.name());
}

//Precondition: All place properties have been checked and are found to be valid.
//Note: thumnbnails and icons are beyond the scope of this function.
void JsonDb::addToJson(QJsonObject *jsonObj, const QPlace &place)
{
    jsonObj->insert(JsonDb::Type, JsonDb::PlaceType);
    if (!place.placeId().isEmpty())
        jsonObj->insert(JsonDb::Uuid, place.placeId());
    if (!place.name().isEmpty())
        jsonObj->insert(JsonDb::Name, place.name());

    QJsonObject locationJson = jsonObj->value(JsonDb::Location).toObject();
    QJsonObject coordJson = jsonObj->value(JsonDb::Coordinate).toObject();

    if (place.location().coordinate() != QGeoCoordinate()) {
        coordJson.insert(JsonDb::Latitude, place.location().coordinate().latitude());
        coordJson.insert(JsonDb::Longitude, place.location().coordinate().longitude());
        locationJson.insert(JsonDb::Coordinate, coordJson);
    } else {
        locationJson.remove(JsonDb::Coordinate);
    }

    QJsonObject addressJson = locationJson.value(JsonDb::Address).toObject();
    if (!place.location().address().street().isEmpty())
        addressJson.insert(JsonDb::Street, place.location().address().street());
    else
        addressJson.remove(JsonDb::Street);

    if (!place.location().address().district().isEmpty())
        addressJson.insert(JsonDb::District, place.location().address().district());
    else
        addressJson.remove(JsonDb::District);

    if (!place.location().address().city().isEmpty())
        addressJson.insert(JsonDb::City, place.location().address().city());
    else
        addressJson.remove(JsonDb::City);

    if (!place.location().address().county().isEmpty())
        addressJson.insert(JsonDb::County, place.location().address().county());
    else
        addressJson.remove(JsonDb::County);

    if (!place.location().address().state().isEmpty())
        addressJson.insert(JsonDb::State, place.location().address().state());
    else
        addressJson.remove(JsonDb::State);

    if (!place.location().address().country().isEmpty())
        addressJson.insert(JsonDb::Country, place.location().address().country());
    else
        addressJson.remove(JsonDb::Country);

    if (!place.location().address().countryCode().isEmpty())
        addressJson.insert(JsonDb::CountryCode, place.location().address().countryCode());
    else
        addressJson.remove(JsonDb::CountryCode);

    if (!place.location().address().postalCode().isEmpty())
        addressJson.insert(JsonDb::PostalCode, place.location().address().postalCode());
    else
        addressJson.remove(JsonDb::PostalCode);

    if (!addressJson.isEmpty())
        locationJson.insert(JsonDb::Address, addressJson);
    else
        locationJson.remove(JsonDb::Address);

    //must always have a location in the schema
    jsonObj->insert(JsonDb::Location, locationJson);

    QStringList contactTypes = place.contactTypes();
    if (contactTypes.contains(QPlaceContactDetail::Phone) || contactTypes.contains(QPlaceContactDetail::Fax)) {
        QJsonArray phonesArray;

        foreach (const QPlaceContactDetail &phone, place.contactDetails(QPlaceContactDetail::Phone)) {
            QJsonObject phoneJson;
            phoneJson.insert(JsonDb::PhoneSubType, JsonDb::SubTypeLandline);
            phoneJson.insert(JsonDb::Label, phone.label());
            phoneJson.insert(JsonDb::Value, phone.value());
            phonesArray.append(phoneJson);
        }

        foreach (const QPlaceContactDetail &fax, place.contactDetails(QPlaceContactDetail::Fax)) {
            QJsonObject faxJson;
            faxJson.insert(JsonDb::PhoneSubType, JsonDb::SubTypeFax);
            faxJson.insert(JsonDb::Label, fax.label());
            faxJson.insert(JsonDb::Value, fax.value());
            phonesArray.append(faxJson);
        }

        if (!phonesArray.isEmpty())
            jsonObj->insert(JsonDb::Phones, phonesArray);
    } else {
        jsonObj->remove(JsonDb::Phones);
    }

    if (contactTypes.contains(QPlaceContactDetail::Website)) {
        QJsonArray websitesArray;
        foreach (const QPlaceContactDetail &website, place.contactDetails(QPlaceContactDetail::Website)) {
            QJsonObject websiteJson;
            websiteJson.insert(JsonDb::Label, website.label());
            websiteJson.insert(JsonDb::Url, website.value());
            websitesArray.append(websiteJson);
        }

        if (!websitesArray.isEmpty())
            jsonObj->insert(JsonDb::Websites, websitesArray);
    } else {
        jsonObj->remove(JsonDb::Websites);
    }

    if (contactTypes.contains(QPlaceContactDetail::Email)) {
        QJsonArray emailsArray;
        foreach (const QPlaceContactDetail &email, place.contactDetails(QPlaceContactDetail::Email)) {
            QJsonObject emailJson;
            emailJson.insert(JsonDb::Label, email.label());
            emailJson.insert(JsonDb::Value, email.value());
            emailsArray.append(emailJson);
        }

        if (!emailsArray.isEmpty())
            jsonObj->insert(JsonDb::Emails, emailsArray);
    } else {
        jsonObj->remove(JsonDb::Emails);
    }

    if (!place.categories().isEmpty()) {
        QJsonArray categoryUuidsArray;
        foreach (const QPlaceCategory &category, place.categories())
            categoryUuidsArray.append(category.categoryId());
        jsonObj->insert(JsonDb::CategoryUuids, categoryUuidsArray);
        //note all category uuids is set elsewhere
    } else {
        jsonObj->remove(JsonDb::CategoryUuids);
        jsonObj->remove(JsonDb::AllCategoryUuids);
    }

    if (!place.extendedAttributeTypes().isEmpty()) {
        QJsonObject attributesJson;
        //note this is not a list but an object containing the attributes
        foreach (const QString attributeType, place.extendedAttributeTypes()) {
                QJsonObject attributeJson;
                attributeJson.insert(JsonDb::Label, place.extendedAttribute(attributeType).label());
                attributeJson.insert(JsonDb::Text, place.extendedAttribute(attributeType).text());
                attributesJson.insert(attributeType, attributeJson);
        }
        if (!attributesJson.isEmpty())
            jsonObj->insert(JsonDb::ExtendedAttributes, attributesJson);
    } else {
        jsonObj->remove(JsonDb::ExtendedAttributes);
    }

    jsonObj->insert(JsonDb::VisibilityScope, JsonDb::DeviceVisibility);
}

QJsonObject JsonDb::convertToJson(const QPlace &place)
{
    QJsonObject jsonObj;

    jsonObj.insert(JsonDb::Type, JsonDb::PlaceType);
    if (!place.placeId().isEmpty())
        jsonObj.insert(JsonDb::Uuid, place.placeId());
    if (!place.name().isEmpty())
        jsonObj.insert(JsonDb::Name, place.name());

    QJsonObject locationJson;
    if (place.location().coordinate() != QGeoCoordinate()) {
        QJsonObject coordJson;
        coordJson.insert(JsonDb::Latitude, place.location().coordinate().latitude());
        coordJson.insert(JsonDb::Longitude, place.location().coordinate().longitude());
        locationJson.insert(JsonDb::Coordinate, coordJson);
    }

    if (!place.location().address().isEmpty()) {
        QJsonObject addressJson;
        if (!place.location().address().street().isEmpty())
            addressJson.insert(JsonDb::Street, place.location().address().street());

        if (!place.location().address().district().isEmpty())
            addressJson.insert(JsonDb::District, place.location().address().district());

        if (!place.location().address().city().isEmpty())
            addressJson.insert(JsonDb::City, place.location().address().city());

        if (!place.location().address().county().isEmpty())
            addressJson.insert(JsonDb::County, place.location().address().county());

        if (!place.location().address().state().isEmpty())
            addressJson.insert(JsonDb::State, place.location().address().state());

        if (!place.location().address().country().isEmpty())
            addressJson.insert(JsonDb::Country, place.location().address().country());

        if (!place.location().address().countryCode().isEmpty())
            addressJson.insert(JsonDb::CountryCode, place.location().address().countryCode());

        if (!place.location().address().postalCode().isEmpty())
            addressJson.insert(JsonDb::PostalCode, place.location().address().postalCode());

        locationJson.insert(JsonDb::Address, addressJson);
    }

    // TODO: Q_ASSERT(locationJson.isEmpty()); //it expected that at least one of the coordinates or
                                      //address has been filled in.
    jsonObj.insert(JsonDb::Location, locationJson);

    QStringList contactTypes = place.contactTypes();
    if (contactTypes.contains(QPlaceContactDetail::Phone) || contactTypes.contains(QPlaceContactDetail::Fax)) {
        QJsonArray phonesArray;

        foreach (const QPlaceContactDetail &phone, place.contactDetails(QPlaceContactDetail::Phone)) {
            QJsonObject phoneJson;
            phoneJson.insert(JsonDb::PhoneSubType, JsonDb::SubTypeLandline);
            phoneJson.insert(JsonDb::Label, phone.label());
            phoneJson.insert(JsonDb::Value, phone.value());
            phonesArray.append(phoneJson);
        }

        foreach (const QPlaceContactDetail &fax, place.contactDetails(QPlaceContactDetail::Fax)) {
            QJsonObject faxJson;
            faxJson.insert(JsonDb::PhoneSubType, JsonDb::SubTypeFax);
            faxJson.insert(JsonDb::Label, fax.label());
            faxJson.insert(JsonDb::Value, fax.value());
            phonesArray.append(faxJson);
        }

        if (!phonesArray.isEmpty())
            jsonObj.insert(JsonDb::Phones, phonesArray);
    } else {
        jsonObj.remove(JsonDb::Phones);
    }

    if (contactTypes.contains(QPlaceContactDetail::Website)) {
        QJsonArray websitesArray;
        foreach (const QPlaceContactDetail &website, place.contactDetails(QPlaceContactDetail::Website)) {
            QJsonObject websiteJson;
            websiteJson.insert(JsonDb::Label, website.label());
            websiteJson.insert(JsonDb::Url, website.value());
            websitesArray.append(websiteJson);
        }

        if (!websitesArray.isEmpty())
            jsonObj.insert(JsonDb::Websites, websitesArray);
    } else {
        jsonObj.remove(JsonDb::Websites);
    }

    if (contactTypes.contains(QPlaceContactDetail::Email)) {
        QJsonArray emailsArray;
        foreach (const QPlaceContactDetail &email, place.contactDetails(QPlaceContactDetail::Email)) {
            QJsonObject emailJson;
            emailJson.insert(JsonDb::Label, email.label());
            emailJson.insert(JsonDb::Value, email.value());
            emailsArray.append(emailJson);
        }

        if (!emailsArray.isEmpty())
            jsonObj.insert(JsonDb::Emails, emailsArray);
    } else {
        jsonObj.remove(JsonDb::Emails);
    }

    if (!place.categories().isEmpty()) {
        QJsonArray categoryUuidsArray;
        foreach (const QPlaceCategory &category, place.categories())
            categoryUuidsArray.append(category.categoryId());
        jsonObj.insert(JsonDb::CategoryUuids, categoryUuidsArray);
    }

    if (!place.extendedAttributeTypes().isEmpty()) {
        QJsonObject attributesJson;//note this is not a list but an object containing the attributes

        foreach (const QString attributeType, place.extendedAttributeTypes()) {
            if (attributeType.startsWith(QLatin1String("x_id"))) {
                QJsonObject attributeJson;
                attributeJson.insert(JsonDb::Label, place.extendedAttribute(attributeType).label());
                attributeJson.insert(JsonDb::Text, place.extendedAttribute(attributeType).text());
                attributesJson.insert(attributeType, attributeJson);
            }
        }

        if (!attributesJson.isEmpty())
            jsonObj.insert(JsonDb::ExtendedAttributes, attributesJson);
    }

    return jsonObj;
}

/* Expected response format
   {
   "data": <array of json objects>
   "explantion":
   "length": <integer>
   "offset": <integer>
   }
*/
QList<QPlace> JsonDb::convertJsonObjectsToPlaces(const QList<QJsonObject> &objects,
                                                         const QPlaceManagerEngineJsonDb *engine)
{
    QList <QPlace> places;
    foreach (const QJsonObject &object, objects)
        places.append(JsonDb::convertJsonObjectToPlace(object,engine));

    return places;
}

QList<QPlaceCategory> JsonDb::convertJsonObjectsToCategories(const QList<QJsonObject> &objects,
                                                        const QPlaceManagerEngineJsonDb *engine)
{
    QList<QPlaceCategory> categories;
    foreach (const QJsonObject &object, objects)
        categories.append(JsonDb::convertJsonObjectToCategory(object,engine));

    return categories;
}

//Note the place category ids are set by this function.
//Assigning category properties such as name etc must be handled elsewhere.
QPlace JsonDb::convertJsonObjectToPlace(const QJsonObject &placeJson,
                                            const QPlaceManagerEngineJsonDb *engine)
{
    QPlace place;
    place.setName(placeJson.value(JsonDb::Name).toString());
    place.setPlaceId(placeJson.value(JsonDb::Uuid).toString());

    QJsonObject locationJson = placeJson.value(JsonDb::Location).toObject();

    QGeoLocation location;
    if (locationJson.contains(JsonDb::Coordinate)) {
        //Note if a coordinate exists, it should not be a nan value.
        QGeoCoordinate coord;

        QJsonObject coordJson = locationJson.value(JsonDb::Coordinate).toObject();
        coord.setLatitude(coordJson.value(JsonDb::Latitude).toDouble());
        coord.setLongitude(coordJson.value(JsonDb::Longitude).toDouble());

        location.setCoordinate(coord);
    }

    if (locationJson.contains(JsonDb::Address)) {
        QGeoAddress address;

        QJsonObject addressJson = locationJson.value(JsonDb::Address).toObject();
        address.setStreet(addressJson.value(JsonDb::Street).toString());
        address.setDistrict(addressJson.value(JsonDb::District).toString());
        address.setCity(addressJson.value(JsonDb::City).toString());
        address.setCounty(addressJson.value(JsonDb::County).toString());
        address.setState(addressJson.value(JsonDb::State).toString());
        address.setCountry(addressJson.value(JsonDb::Country).toString());
        address.setCountryCode(addressJson.value(JsonDb::CountryCode).toString());
        address.setPostalCode(addressJson.value(JsonDb::PostalCode).toString());

        location.setAddress(address);
    }

    place.setLocation(location);

    QJsonArray phonesArray = placeJson.value(JsonDb::Phones).toArray();
    for (int i = 0; i < phonesArray.count(); ++i) {
        QPlaceContactDetail detail;
        detail.setLabel(phonesArray.at(i).toObject().value(JsonDb::Label).toString());
        detail.setValue(phonesArray.at(i).toObject().value(JsonDb::Value).toString());

        QString phoneType = phonesArray.at(i).toObject().value(JsonDb::PhoneSubType).toString();
        if (phoneType == JsonDb::SubTypeLandline)
            place.appendContactDetail(QPlaceContactDetail::Phone, detail);
        else if (phoneType == JsonDb::SubTypeFax)
            place.appendContactDetail(QPlaceContactDetail::Fax, detail);
    }

    QJsonArray websitesArray = placeJson.value(JsonDb::Websites).toArray();
    for (int i = 0; i < websitesArray.count(); ++i) {
        QPlaceContactDetail detail;
        detail.setLabel(websitesArray.at(i).toObject().value(JsonDb::Label).toString());
        detail.setValue(websitesArray.at(i).toObject().value(JsonDb::Url).toString());
        place.appendContactDetail(QPlaceContactDetail::Website, detail);
    }

    QJsonArray emailsArray = placeJson.value(JsonDb::Emails).toArray();
    for (int i = 0; i < emailsArray.count(); ++i) {
        QPlaceContactDetail detail;
        detail.setLabel(emailsArray.at(i).toObject().value(JsonDb::Label).toString());
        detail.setValue(emailsArray.at(i).toObject().value(JsonDb::Value).toString());
        place.appendContactDetail(QPlaceContactDetail::Email, detail);
    }

    QList<QPlaceCategory> categories;
    QJsonArray categoryUuidsJson = placeJson.value(JsonDb::CategoryUuids).toArray();
    for (int i = 0; i < categoryUuidsJson.count(); ++i) {
        QPlaceCategory cat;
        cat.setCategoryId(categoryUuidsJson.at(i).toString());
        categories.append(cat);
    }

    place.setCategories(categories);

    if (placeJson.contains(JsonDb::ExtendedAttributes)) {
        QJsonObject attributesJson = placeJson.value(JsonDb::ExtendedAttributes).toObject();
        foreach (const QString &attributeType, attributesJson.keys()) {
            QJsonObject attributeJson = attributesJson.value(attributeType).toObject();
            QPlaceAttribute attribute;
            attribute.setLabel(attributeJson.value(JsonDb::Label).toString());
            attribute.setText(attributeJson.value(JsonDb::Text).toString());
            place.setExtendedAttribute(attributeType, attribute);
        }
    }

    QPlaceIcon icon = convertJsonObjectToIcon(placeJson.value(JsonDb::Thumbnails).toObject(), engine);
    place.setIcon(icon);

    place.setVisibility(QtLocation::DeviceVisibility);

    return place;
}

QPlaceCategory JsonDb::convertJsonObjectToCategory(const QJsonObject &categoryJson,
                                                       const QPlaceManagerEngineJsonDb *engine)
{
    Q_UNUSED(engine);

    QPlaceCategory category;
    if (categoryJson.value(JsonDb::Type) == JsonDb::CategoryType) {
        category.setName(categoryJson.value(JsonDb::Name).toString());
        category.setCategoryId(categoryJson.value(JsonDb::Uuid).toString());
    }

    QPlaceIcon icon = convertJsonObjectToIcon(categoryJson.value(JsonDb::Thumbnails).toObject(), engine);
    category.setIcon(icon);
    return category;
}

QPlaceIcon JsonDb::convertJsonObjectToIcon(const QJsonObject &thumbnailsJson, const QPlaceManagerEngineJsonDb *engine)
{
    QVariantMap iconParameters;
    QList<QLatin1String> sizes;
    sizes << JsonDb::Small << JsonDb::Medium << JsonDb::Large << JsonDb::Fullscreen;

    foreach (const QLatin1String &size, sizes) {
        if (thumbnailsJson.contains(size)) {
            QJsonObject thumbnailJson = thumbnailsJson.value(size).toObject();
            iconParameters.insert(QString(size) + QLatin1String("Url"), QUrl::fromUserInput(thumbnailJson.value(JsonDb::Url).toString()));
            QSize sizeDimensions = QSize(thumbnailJson.value(JsonDb::Width).toDouble(), thumbnailJson.value(JsonDb::Height).toDouble());
            iconParameters.insert(QString(size) + QLatin1String("Size"), sizeDimensions);
        }
    }

    QPlaceIcon icon;
    if (!iconParameters.isEmpty()) {
        icon.setParameters(iconParameters);
        icon.setManager(engine->manager());
    }

    return icon;
}

void JsonDb::makeConnections(QJsonDbRequest *request, QObject *parent, const char *slot)
{
    if (slot)
        QObject::connect(request, SIGNAL(finished()), parent, slot);
    else
        QObject::connect(request, SIGNAL(finished()), parent, SLOT(requestFinished()));//TODO: eventually remove this
    QObject::connect(request, SIGNAL(finished()), request, SLOT(deleteLater()));
    QObject::connect(request, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
            parent, SLOT(requestError(QtJsonDb::QJsonDbRequest::ErrorCode,QString)));
    QObject::connect(request, SIGNAL(error(QtJsonDb::QJsonDbRequest::ErrorCode,QString)),
            request, SLOT(deleteLater()));
}

void JsonDb::getCategory(const QString &uuid, QObject *parent)
{
    QJsonDbReadRequest *request = new QJsonDbReadRequest(parent);
    request->setQuery(QStringLiteral("[?_type=%type][?_uuid=%uuid]"));
    request->bindValue(QStringLiteral("type"), JsonDb::CategoryType);
    request->bindValue(QStringLiteral("uuid"), uuid);

    makeConnections(request, parent);
    m_connection->send(request);
}

void JsonDb::getCategories(const QList<QPlaceCategory> &categories, QObject *parent, const char *slot)
{
    QStringList categoryUuids;
    foreach (const QPlaceCategory &category, categories)
        categoryUuids.append(category.categoryId());

    getCategories(categoryUuids, parent, slot);
}

void JsonDb::getCategories(const QStringList &uuids, QObject *parent, const char *slot)
{
    QJsonDbReadRequest *request = new QJsonDbReadRequest(parent);
    request->setQuery(QStringLiteral("[?_type=%type][?_uuid in %categoryUuids]"));
    request->bindValue(QStringLiteral("type"), JsonDb::CategoryType);
    request->bindValue(QStringLiteral("categoryUuids"), QJsonArray::fromStringList(uuids));

    JsonDb::makeConnections(request, parent, slot);
    m_connection->send(request);
}

void JsonDb::getChildCategories(const QString &uuid, QObject *parent, const char *slot)
{
    QJsonDbReadRequest *request = new QJsonDbReadRequest(parent);
    request->setQuery(QStringLiteral("[?_type=%type][?") + JsonDb::CategoryParentId + QStringLiteral("=%uuid]"));
    request->bindValue(QStringLiteral("type"), JsonDb::CategoryType);
    request->bindValue(QStringLiteral("uuid"), uuid);

    makeConnections(request, parent, slot);
    m_connection->send(request);
}

void JsonDb::getCategory(const QString &uuid, QObject *parent, const char *slot)
{
    QJsonDbReadRequest *request = new QJsonDbReadRequest(parent);
    request->setQuery(QStringLiteral("[?_type=%type][?_uuid=%uuid]"));
    request->bindValue(QStringLiteral("type"), JsonDb::CategoryType);
    request->bindValue(QStringLiteral("uuid"), uuid);

    makeConnections(request, parent, slot);
    m_connection->send(request);
}

void JsonDb::getPlace(const QString &uuid, QObject *parent, const char *slot)
{
    //TODO:   Optimization use QJsonDbReadObject??
    //    QJsonDbReadObjectRequest *request = new QJsonDbReadObjectRequest(QUuid(m_placeId), this);
    QJsonDbReadRequest *request = new QJsonDbReadRequest(parent);
    request->setQuery(QStringLiteral("[?_type=%type][?_uuid=%uuid]"));
    request->bindValue(QStringLiteral("type"), JsonDb::PlaceType);
    request->bindValue(QStringLiteral("uuid"), uuid);

    makeConnections(request, parent, slot);
    m_connection->send(request);
}

void JsonDb::findPlacesByAlternativeId(const QString externalIdName, const QStringList &alternativeIds,
                                       QObject *parent, const char *slot)
{
    QString extIdPropertyName = QString(JsonDb::ExtendedAttributes) + QLatin1String(".")
            + externalIdName + QLatin1String(".") +
            JsonDb::Text;

    QJsonDbReadRequest *request = new QJsonDbReadRequest(this);
    request->setQuery(QString::fromLatin1("[?_type = %type]")
                      + QString::fromLatin1("[?%1 in %alternativeIds]").arg(extIdPropertyName));
    request->bindValue(QStringLiteral("type"), JsonDb::PlaceType);
    request->bindValue(QStringLiteral("alternativeIds"), QJsonArray::fromStringList(alternativeIds));
    makeConnections(request, parent, slot);
    m_connection->send(request);
}

void JsonDb::findAllPlaces(QObject *parent, const char *slot)
{
    QJsonDbReadRequest *request = new QJsonDbReadRequest(this);
    request->setQuery(QStringLiteral("[?_type= %type]"));
    request->bindValue(QStringLiteral("type"), JsonDb::PlaceType);
    makeConnections(request, parent, slot);
    m_connection->send(request);
}

void JsonDb::read(const QString &query, QObject *parent, const char *slot)
{
    QJsonDbReadRequest *request = new QJsonDbReadRequest(parent);
    request->setQuery(query);
    makeConnections(request, parent, slot);
    m_connection->send(request);
}

void JsonDb::write(const QJsonObject &jsonObject, QObject *parent, const char *slot)
{
    QList<QJsonObject> objects;
    objects << jsonObject;
    write(objects, parent, slot);
}

void JsonDb::write(const QList<QJsonObject> &jsonObjects, QObject *parent, const char *slot)
{
    QList<QJsonObject> objects = jsonObjects;

    for (int i = 0;i < objects.count(); ++i) {
        if (!objects[i].contains(JsonDb::Uuid))
            objects[i].insert(JsonDb::Uuid, QUuid::createUuid().toString());
    }

    QJsonDbWriteRequest *writeRequest = new QJsonDbWriteRequest(parent);
    writeRequest->setObjects(objects);
    makeConnections(writeRequest, parent, slot);
    m_connection->send(writeRequest);
}

void JsonDb::remove(const QJsonObject &jsonObject, QObject *parent, const char *slot)
{
    QJsonDbRemoveRequest *removeRequest = new QJsonDbRemoveRequest(jsonObject, parent);
    JsonDb::makeConnections(removeRequest, parent, slot);
    m_connection->send(removeRequest);
}

void JsonDb::remove(const QList<QJsonObject> &jsonObjects, QObject *parent, const char *slot)
{
    QJsonDbRemoveRequest *removeRequest = new QJsonDbRemoveRequest(jsonObjects, parent);
    JsonDb::makeConnections(removeRequest, parent, slot);
    m_connection->send(removeRequest);
}

void JsonDb::searchForPlaces(const QPlaceSearchRequest &request, QObject *parent, const char *slot)
{
    QString queryString;
    if (!request.searchTerm().isEmpty()) {
        queryString += QString::fromLatin1("[?%1=\"%2\"][?%3 =~ \"!.*%4.*!i\"]")
                        .arg(JsonDb::Type).arg(JsonDb::PlaceType).arg(JsonDb::Name).arg(request.searchTerm());
    }

    if (queryString.isEmpty())
        queryString = QString::fromLatin1("[?%1 = \"%2\"]").arg(JsonDb::Type).arg(JsonDb::PlaceType);

    if (request.relevanceHint() == QPlaceSearchRequest::LexicalPlaceNameHint)
        queryString += QString::fromLatin1("[/") + JsonDb::Name + QLatin1String("]");

    if (!request.categories().isEmpty()) {
        for (int i = 0;  i < request.categories().count(); ++i) {
            if (i == 0) {
                queryString += QString::fromLatin1("[?%1 contains \"%2\" ")
                        .arg(JsonDb::AllCategoryUuids).arg(request.categories().at(i).categoryId());
            } else {
                queryString += QString::fromLatin1("| ?%1 contains \"%2\" ")
                .arg(JsonDb::AllCategoryUuids).arg(request.categories().at(i).categoryId());
            }
        }
        queryString += QString::fromLatin1("]");
    }

    QJsonDbReadRequest *jsonDbRequest = new QJsonDbReadRequest(parent);
    jsonDbRequest->setQuery(queryString);
    makeConnections(jsonDbRequest, parent, slot);
    m_connection->send(jsonDbRequest);
}

QStringList JsonDb::categoryIds(QList<QPlace> places)
{
    QStringList uuids;
    foreach (const QPlace &place, places) {
        foreach (const QPlaceCategory &category, place.categories()) {
            if (!category.categoryId().isEmpty() && !uuids.contains(category.categoryId()))
                uuids.append(category.categoryId());
        }
    }

    return uuids;
}

void JsonDb::processPlaceNotifications()
{
    emit placeNotifications(m_placeWatcher->takeNotifications());
}

void JsonDb::processCategoryNotifications()
{
    emit categoryNotifications(m_categoryWatcher->takeNotifications());
}
