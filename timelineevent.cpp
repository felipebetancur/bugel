#include "timelineevent.h"

#include <QDebug>
#include <QJsonArray>

#include "project.h"

QJsonObject EventType::toJSON() const
{
    QJsonArray props;
    for (auto it = properties.begin(); it != properties.end(); it++) {
        QJsonObject prop;
        prop["name"] = it.key();
        prop["type"] = QMetaType::typeName(it->type);
        prop["defaultValue"] = it->defaultValue.toString();
        props.push_back(prop);
    }

    QJsonObject type;
    type["name"] = name.c_str();
    type["properties"] = props;
    return type;
}

void EventType::fromJSON(const QJsonObject& type)
{
    name = type["name"].toString().toUtf8().constData();

    const QJsonArray& props = type["properties"].toArray();
    for (auto it = props.begin(); it != props.end(); it++) {
        const QJsonObject& prop = it->toObject();
        QMetaType::Type type = (QMetaType::Type)QMetaType::type(prop["type"].toString().toUtf8().constData());

        properties[prop["name"].toString()] =
                EventProperty(type, prop["defaultValue"].toString());
    }
}

TimelineEvent::TimelineEvent(const std::shared_ptr<EventType>& type, double time)
    : mTime(time), mType(type)
{
    // initialize properties to defaults
    if (mType) {
        const auto& props = mType->properties;
        for (auto it = props.begin(); it != props.end(); it++) {
            mProperties[it.key()] = it->defaultValue;

            // make sure the default value is the right type
            bool conv = mProperties[it.key()].convert(it->type);
            // Q_ASSERT(conv);
        }
    }
}

QJsonObject TimelineEvent::toJSON() const
{
    QJsonObject event;
    event["type"] = typeName();
    event["time"] = time();

    // write properties
    for (auto it = mProperties.begin(); it != mProperties.end(); it++) {
        event[it.key()] = it->toString();
    }

    return event;
}

std::shared_ptr<TimelineEvent> TimelineEvent::createFromJSON(const QJsonObject& event)
{
    const QString typeName = event["type"].toString();
    const double time = event["time"].toDouble();

    std::shared_ptr<EventType> type = Project::get()->eventType(typeName);

    std::shared_ptr<TimelineEvent> ev = std::make_shared<TimelineEvent>(type, time);

    // read properties
    for (auto it = ev->mProperties.begin(); it != ev->mProperties.end(); it++) {
        *it = event[it.key()].toString();
        bool conv = it->convert(type->properties.find(it.key())->type);
        Q_ASSERT(conv);
    }

    return ev;
}

void TimelineEvent::setProperty(const QString &name, const QVariant &value) {
    // Do we have a property by this name?
    auto it = mProperties.find(name);
    if (it == mProperties.end())
        throw EventException() << "Property \"" << name <<
                                  "\" does not exist for " << mType->name <<
                                  " events!";

    // Can whatever we were given be converted to the type we require?
    const auto propType = mType->properties.find(name)->type;
    if (!value.canConvert(propType))
        throw EventException() << "Cannot set property \"" << name <<
                                  " to type " << value.typeName() << "!";

    // Set it, but keep it as whatever type it was given.
    it->setValue(value);

    // Force a conversion to the type we require.
    // This can fail even if canConvert() returns true - for example,
    // imagine converting a string to a date. That conversion is possible,
    // but if the string is "loldicks," we can't convert it to a date.
    // So, we do this to make sure it's a well-formed version of the type we require.
    if (!it->convert(propType)) {
        *it = mType->properties.find(name)->defaultValue;
        throw EventException() << "Invalid data for property \"" << name <<
                                  "\" to convert to " << QMetaType::typeName(propType);
    }
}
