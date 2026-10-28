#include "mauisecretstore.h"

#include <QDebug>
#include <QRegularExpression>
#include <QVector>

#ifdef signals
#pragma push_macro("signals")
#undef signals
#define MAUI_ACCOUNTS_RESTORE_QT_SIGNALS
#endif
extern "C" {
#include <libsecret/secret.h>
}
#ifdef MAUI_ACCOUNTS_RESTORE_QT_SIGNALS
#pragma pop_macro("signals")
#undef MAUI_ACCOUNTS_RESTORE_QT_SIGNALS
#endif

namespace
{
const QStringList sharedAttributes = {
    QStringLiteral("application"),
    QStringLiteral("kind"),
    QStringLiteral("host"),
    QStringLiteral("username"),
    QStringLiteral("protocol"),
    QStringLiteral("address"),
    QStringLiteral("resource")
};

const SecretSchema *sharedSchema()
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    static const SecretSchema schema = {
        "org.maui.accounts.credentials",
        SECRET_SCHEMA_NONE,
        {
            { "application", SECRET_SCHEMA_ATTRIBUTE_STRING },
            { "kind",        SECRET_SCHEMA_ATTRIBUTE_STRING },
            { "host",        SECRET_SCHEMA_ATTRIBUTE_STRING },
            { "username",    SECRET_SCHEMA_ATTRIBUTE_STRING },
            { "protocol",    SECRET_SCHEMA_ATTRIBUTE_STRING },
            { "address",     SECRET_SCHEMA_ATTRIBUTE_STRING },
            { "resource",    SECRET_SCHEMA_ATTRIBUTE_STRING },
            { nullptr,       SECRET_SCHEMA_ATTRIBUTE_STRING }
        }
    };
#pragma GCC diagnostic pop

    return &schema;
}

bool validValue(const QString &value)
{
    return !value.contains(QChar(0))
        && !value.contains(QRegularExpression(QStringLiteral("[\\x00-\\x1F\\x7F]")));
}

bool validAttributes(const QVariantMap &attributes, bool requireIdentity)
{
    for (auto it = attributes.constBegin(); it != attributes.constEnd(); ++it) {
        if (!sharedAttributes.contains(it.key()) || !validValue(it.value().toString()))
            return false;
    }

    if (requireIdentity)
        return !attributes.value(QStringLiteral("application")).toString().isEmpty()
            && !attributes.value(QStringLiteral("kind")).toString().isEmpty();

    return true;
}

GHashTable *attributeTable(const QVariantMap &attributes)
{
    auto *table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    for (auto it = attributes.constBegin(); it != attributes.constEnd(); ++it) {
        const auto key = it.key().toUtf8();
        const auto value = it.value().toString().toUtf8();
        g_hash_table_insert(table, g_strdup(key.constData()), g_strdup(value.constData()));
    }
    return table;
}

QVariantMap attributesFromItem(GHashTable *attributes)
{
    QVariantMap result;
    if (!attributes)
        return result;

    GHashTableIter iterator;
    gpointer key = nullptr;
    gpointer value = nullptr;
    g_hash_table_iter_init(&iterator, attributes);
    while (g_hash_table_iter_next(&iterator, &key, &value)) {
        result[QString::fromUtf8(static_cast<const char *>(key))] = QString::fromUtf8(static_cast<const char *>(value));
    }
    return result;
}

SecretSchema legacySchema(const QString &name, const QStringList &attributeNames, QVector<QByteArray> *names)
{
    SecretSchema schema = {};
    names->reserve(attributeNames.size() + 1);
    names->append(name.toUtf8());
    schema.name = names->first().constData();

    for (int i = 0; i < attributeNames.size() && i < 31; ++i) {
        names->append(attributeNames.at(i).toUtf8());
        schema.attributes[i].name = names->last().constData();
        schema.attributes[i].type = SECRET_SCHEMA_ATTRIBUTE_STRING;
    }

    return schema;
}

QString credentialLabel(const QString &application, const QString &kind, const QVariantMap &attributes)
{
    const auto identity = attributes.value(QStringLiteral("resource"), attributes.value(QStringLiteral("host"))).toString();
    const auto username = attributes.value(QStringLiteral("username")).toString();
    return QStringLiteral("%1 %2: %3%4").arg(application, kind, username.isEmpty() ? QString() : username + QStringLiteral(" @ "), identity);
}
}

MauiSecretStore *MauiSecretStore::instance()
{
    static MauiSecretStore store;
    return &store;
}

MauiSecretStore::MauiSecretStore(QObject *parent)
    : QObject(parent)
{}

bool MauiSecretStore::available() const
{
    GError *error = nullptr;
    auto *service = secret_service_get_sync(SECRET_SERVICE_NONE, nullptr, &error);
    if (error) {
        g_error_free(error);
        return false;
    }

    if (!service)
        return false;

    g_object_unref(service);
    return true;
}

bool MauiSecretStore::save(const QString &application,
                           const QString &kind,
                           const QVariantMap &attributes,
                           const QString &secret,
                           const QString &label)
{
    QVariantMap storedAttributes = attributes;
    storedAttributes.insert(QStringLiteral("application"), application);
    storedAttributes.insert(QStringLiteral("kind"), kind);
    if (!validAttributes(storedAttributes, true) || !validValue(secret))
        return false;

    GError *error = nullptr;
    auto *service = secret_service_get_sync(SECRET_SERVICE_NONE, nullptr, &error);
    if (!service) {
        if (error)
            g_error_free(error);
        return false;
    }

    auto *attributesTable = attributeTable(storedAttributes);
    auto *value = secret_value_new(secret.toUtf8().constData(), -1, "text/plain");
    const auto itemLabel = label.isEmpty() ? credentialLabel(application, kind, attributes) : label;
    const bool result = secret_service_store_sync(service,
                                                  sharedSchema(),
                                                  attributesTable,
                                                  SECRET_COLLECTION_DEFAULT,
                                                  itemLabel.toUtf8().constData(),
                                                  value,
                                                  nullptr,
                                                  &error);

    secret_value_unref(value);
    g_hash_table_unref(attributesTable);
    g_object_unref(service);

    if (error) {
        qWarning() << "MauiSecretStore::save:" << error->message;
        g_error_free(error);
        return false;
    }

    return result;
}

QVariantList MauiSecretStore::findAll(const QString &application,
                                      const QString &kind,
                                      const QVariantMap &attributes) const
{
    QVariantMap searchAttributes = attributes;
    searchAttributes.insert(QStringLiteral("application"), application);
    searchAttributes.insert(QStringLiteral("kind"), kind);
    if (!validAttributes(searchAttributes, true))
        return {};

    GError *error = nullptr;
    auto *service = secret_service_get_sync(SECRET_SERVICE_NONE, nullptr, &error);
    if (!service) {
        if (error)
            g_error_free(error);
        return {};
    }

    auto *attributesTable = attributeTable(searchAttributes);
    auto *items = secret_service_search_sync(service,
                                             sharedSchema(),
                                             attributesTable,
                                             static_cast<SecretSearchFlags>(SECRET_SEARCH_ALL | SECRET_SEARCH_UNLOCK | SECRET_SEARCH_LOAD_SECRETS),
                                             nullptr,
                                             &error);
    g_hash_table_unref(attributesTable);

    QVariantList result;
    if (error) {
        qWarning() << "MauiSecretStore::findAll:" << error->message;
        g_error_free(error);
    } else {
        for (auto *item = items; item; item = item->next) {
            auto *secretItem = SECRET_ITEM(item->data);
            auto *value = secret_item_get_secret(secretItem);
            auto *itemAttributes = secret_item_get_attributes(secretItem);
            auto entry = attributesFromItem(itemAttributes);
            if (value) {
                entry.insert(QStringLiteral("secret"), QString::fromUtf8(secret_value_get(value, nullptr)));
                secret_value_unref(value);
            }
            if (itemAttributes)
                g_hash_table_unref(itemAttributes);
            result.append(entry);
        }
    }

    g_list_free_full(items, g_object_unref);
    g_object_unref(service);
    return result;
}

QVariantMap MauiSecretStore::find(const QString &application,
                                  const QString &kind,
                                  const QVariantMap &attributes) const
{
    const auto entries = findAll(application, kind, attributes);
    if (entries.isEmpty())
        return {{QStringLiteral("found"), false}};

    auto result = entries.first().toMap();
    result.insert(QStringLiteral("found"), true);
    return result;
}

bool MauiSecretStore::remove(const QString &application,
                             const QString &kind,
                             const QVariantMap &attributes)
{
    QVariantMap storedAttributes = attributes;
    storedAttributes.insert(QStringLiteral("application"), application);
    storedAttributes.insert(QStringLiteral("kind"), kind);
    if (!validAttributes(storedAttributes, true))
        return false;

    GError *error = nullptr;
    auto *service = secret_service_get_sync(SECRET_SERVICE_NONE, nullptr, &error);
    if (!service) {
        if (error)
            g_error_free(error);
        return false;
    }

    auto *attributesTable = attributeTable(storedAttributes);
    const bool result = secret_service_clear_sync(service, sharedSchema(), attributesTable, nullptr, &error);
    g_hash_table_unref(attributesTable);
    g_object_unref(service);

    if (error) {
        qWarning() << "MauiSecretStore::remove:" << error->message;
        g_error_free(error);
        return false;
    }

    return result;
}

QVariantList MauiSecretStore::findLegacy(const QString &schemaName,
                                         const QStringList &attributeNames) const
{
    if (schemaName.isEmpty() || attributeNames.isEmpty() || attributeNames.size() > 31)
        return {};

    QVector<QByteArray> names;
    auto schema = legacySchema(schemaName, attributeNames, &names);
    GError *error = nullptr;
    auto *items = secret_service_search_sync(nullptr,
                                             &schema,
                                             nullptr,
                                             static_cast<SecretSearchFlags>(SECRET_SEARCH_ALL | SECRET_SEARCH_UNLOCK | SECRET_SEARCH_LOAD_SECRETS),
                                             nullptr,
                                             &error);

    QVariantList result;
    if (error) {
        qWarning() << "MauiSecretStore::findLegacy:" << error->message;
        g_error_free(error);
        return result;
    }

    for (auto *item = items; item; item = item->next) {
        auto *secretItem = SECRET_ITEM(item->data);
        auto *value = secret_item_get_secret(secretItem);
        auto *itemAttributes = secret_item_get_attributes(secretItem);
        auto entry = attributesFromItem(itemAttributes);
        if (value) {
            entry.insert(QStringLiteral("secret"), QString::fromUtf8(secret_value_get(value, nullptr)));
            secret_value_unref(value);
        }
        if (itemAttributes)
            g_hash_table_unref(itemAttributes);
        result.append(entry);
    }

    g_list_free_full(items, g_object_unref);
    return result;
}

bool MauiSecretStore::removeLegacy(const QString &schemaName,
                                   const QStringList &attributeNames,
                                   const QVariantMap &attributes) const
{
    QVector<QByteArray> names;
    auto schema = legacySchema(schemaName, attributeNames, &names);
    auto *attributesTable = attributeTable(attributes);
    GError *error = nullptr;
    const bool result = secret_service_clear_sync(nullptr, &schema, attributesTable, nullptr, &error);
    g_hash_table_unref(attributesTable);

    if (error) {
        qWarning() << "MauiSecretStore::removeLegacy:" << error->message;
        g_error_free(error);
        return false;
    }

    return result;
}

bool MauiSecretStore::migrateLegacy(const QString &legacySchema,
                                    const QStringList &legacyAttributes,
                                    const QString &application,
                                    const QString &kind)
{
    bool success = true;
    for (const auto &entryValue : findLegacy(legacySchema, legacyAttributes)) {
        const auto entry = entryValue.toMap();
        const auto secret = entry.value(QStringLiteral("secret")).toString();
        QVariantMap attributes;
        for (const auto &attributeName : legacyAttributes)
            if (entry.contains(attributeName))
                attributes.insert(attributeName, entry.value(attributeName));
        if (!save(application, kind, attributes, secret)) {
            success = false;
            continue;
        }

        if (!removeLegacy(legacySchema, legacyAttributes, attributes))
            success = false;
    }

    return success;
}
