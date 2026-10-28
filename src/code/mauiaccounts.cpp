/*
 * <one line to give the program's name and a brief idea of what it does.>
 * Copyright (C) 2019  camilo <chiguitar@unal.edu.co>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "mauiaccounts.h"
#include <QDateTime>
#include <QSqlQuery>
#include <QUrl>
#include "accountsdb.h"
#include "mauisecretstore.h"

namespace {
QVariantMap credentialAttributes(const QString &resource, const QString &username) { return {{QStringLiteral("resource"), resource}, {QStringLiteral("username"), username}}; }
QString secretValue(const QString &kind, const QString &resource, const QString &username) { const auto result = MauiSecretStore::instance()->find(QStringLiteral("mauikit"), kind, credentialAttributes(resource, username)); return result.value(QStringLiteral("secret")).toString(); }
bool hasColumn(AccountsDB *db, const QString &table, const QString &column) { auto query = db->getQuery(QStringLiteral("PRAGMA table_info(%1)").arg(table)); if (!query.exec()) return false; while (query.next()) if (query.value(1).toString().compare(column, Qt::CaseInsensitive) == 0) return true; return false; }}

MauiAccounts *MauiAccounts::m_instance = nullptr;

MauiAccounts::MauiAccounts()
    : MauiList(nullptr)
    , db(new AccountsDB(nullptr))
{
    this->migrateLegacySecrets();
    this->setAccounts();
}

MauiAccounts::~MauiAccounts()
{
    qDebug() << "DELETING MAUI ACCOUNTS INSTANCE";
    this->db->deleteLater();
    this->db = nullptr;
}

const FMH::MODEL_LIST &MauiAccounts::items() const
{
    return this->m_data;
}

void MauiAccounts::migrateLegacySecrets()
{
    auto *secrets = MauiSecretStore::instance();
    if (hasColumn(this->db, QStringLiteral("cloud"), QStringLiteral("password"))) {
        struct CloudRow { QString server; QString user; QString password; };
        QList<CloudRow> rows;
        auto query = this->db->getQuery(QStringLiteral("SELECT server, user, password FROM cloud"));
        if (query.exec()) {
            while (query.next()) rows.append({query.value(0).toString(), query.value(1).toString(), query.value(2).toString()});
            query.finish();
            for (const auto &row : std::as_const(rows)) {
                if (!row.password.isEmpty() && !secrets->save(QStringLiteral("mauikit"), QStringLiteral("cloud"), credentialAttributes(row.server, row.user), row.password)) return;
            }
            auto clear = this->db->getQuery(QStringLiteral("UPDATE cloud SET password = NULL"));
            if (!clear.exec()) {
                qWarning() << "Could not clear legacy cloud passwords:" << clear.lastError().text();
                return;
            }
        } else {
            qWarning() << "Could not read legacy cloud passwords:" << query.lastError().text();
            return;
        }
    }

    const bool oldCalDavSecrets = hasColumn(this->db, QStringLiteral("caldav"), QStringLiteral("password"))
        && hasColumn(this->db, QStringLiteral("caldav"), QStringLiteral("refreshToken"))
        && hasColumn(this->db, QStringLiteral("caldav"), QStringLiteral("clientSecret"));
    if (oldCalDavSecrets) {
        struct CalDavRow { QString url; QString username; QString password; QString refreshToken; QString clientSecret; };
        QList<CalDavRow> rows;
        auto query = this->db->getQuery(QStringLiteral("SELECT url, username, password, refreshToken, clientSecret FROM caldav"));
        if (query.exec()) {
            while (query.next()) rows.append({query.value(0).toString(), query.value(1).toString(), query.value(2).toString(), query.value(3).toString(), query.value(4).toString()});
            query.finish();
            for (const auto &row : std::as_const(rows)) {
                const auto attributes = credentialAttributes(row.url, row.username);
                if (!row.password.isEmpty() && !secrets->save(QStringLiteral("mauikit"), QStringLiteral("caldav-password"), attributes, row.password)) return;
                if (!row.refreshToken.isEmpty() && !secrets->save(QStringLiteral("mauikit"), QStringLiteral("caldav-refresh-token"), attributes, row.refreshToken)) return;
                if (!row.clientSecret.isEmpty() && !secrets->save(QStringLiteral("mauikit"), QStringLiteral("caldav-client-secret"), attributes, row.clientSecret)) return;
            }
            auto clear = this->db->getQuery(QStringLiteral("UPDATE caldav SET password = NULL, refreshToken = NULL, clientSecret = NULL"));
            if (!clear.exec()) {
                qWarning() << "Could not clear legacy CalDAV secrets:" << clear.lastError().text();
                return;
            }
        } else {
            qWarning() << "Could not read legacy CalDAV secrets:" << query.lastError().text();
            return;
        }
    }
}

void MauiAccounts::setAccounts()
{
    Q_EMIT this->preListChanged();
    this->m_data = this->getCloudAccounts();

    Q_EMIT this->countChanged();
    Q_EMIT this->postListChanged();
}

FMH::MODEL_LIST MauiAccounts::getCloudAccounts()
{
    auto accounts = this->get(QStringLiteral("select server, user from cloud"));
    FMH::MODEL_LIST res;
    for (const auto &account : std::as_const(accounts)) {
        const auto map = account.toMap();
        const auto server = map.value(QStringLiteral("server")).toString();
        const auto user = map.value(QStringLiteral("user")).toString();
        res << FMH::MODEL {
            {FMH::MODEL_KEY::ICON, "folder-cloud"},
            {FMH::MODEL_KEY::LABEL, user},
            {FMH::MODEL_KEY::USER, user},
            {FMH::MODEL_KEY::SERVER, server},
            {FMH::MODEL_KEY::PASSWORD, secretValue(QStringLiteral("cloud"), server, user)}};
    }
    return res;
}

QVariantList MauiAccounts::caldavAccountsData()
{
    QVariantList accounts;
    auto query = this->db->getQuery(QStringLiteral("SELECT url, username, authType, tokenUrl, clientId, authorizationUrl, tokenExpiresAt, enabled FROM caldav ORDER BY url"));
    if (!query.exec()) {
        qWarning() << "Could not read CalDAV accounts:" << query.lastError().text();
        return accounts;
    }

    while (query.next()) {
        const auto url = query.value(0).toString();
        const auto username = query.value(1).toString();
        accounts << QVariantMap {
            {QStringLiteral("url"), url},
            {QStringLiteral("username"), username},
            {QStringLiteral("password"), secretValue(QStringLiteral("caldav-password"), url, username)},
            {QStringLiteral("authType"), query.value(2).toString()},
            {QStringLiteral("refreshToken"), secretValue(QStringLiteral("caldav-refresh-token"), url, username)},
            {QStringLiteral("tokenUrl"), query.value(3).toString()},
            {QStringLiteral("clientId"), query.value(4).toString()},
            {QStringLiteral("clientSecret"), secretValue(QStringLiteral("caldav-client-secret"), url, username)},
            {QStringLiteral("authorizationUrl"), query.value(5).toString()},
            {QStringLiteral("tokenExpiresAt"), QDateTime::fromString(query.value(6).toString(), Qt::ISODate)},
            {QStringLiteral("enabled"), query.value(7).toInt() != 0}
        };
    }
    return accounts;
}

QVariantList MauiAccounts::caldavAccounts()
{
    QVariantList accounts;
    for (const auto &accountValue : this->caldavAccountsData()) {
        auto account = accountValue.toMap();
        const auto authType = account.value(QStringLiteral("authType"), QStringLiteral("basic")).toString();
        const auto hasSecret = !account.value(QStringLiteral("password")).toString().isEmpty();
        account.insert(QStringLiteral("hasPassword"), authType != QStringLiteral("bearer") && hasSecret);
        account.insert(QStringLiteral("hasToken"), authType == QStringLiteral("bearer") && hasSecret);
        account.insert(QStringLiteral("hasRefreshToken"), !account.value(QStringLiteral("refreshToken")).toString().isEmpty());
        account.remove(QStringLiteral("password"));
        account.remove(QStringLiteral("refreshToken"));
        account.remove(QStringLiteral("clientSecret"));
        account.remove(QStringLiteral("tokenExpiresAt"));
        accounts.append(account);
    }
    return accounts;
}

bool MauiAccounts::saveCalDavAccount(const QVariantMap &account)
{
    const auto url = account.value(QStringLiteral("url")).toString().trimmed();
    const QUrl parsedUrl(url);
    if (!parsedUrl.isValid() || (parsedUrl.scheme() != QStringLiteral("http") && parsedUrl.scheme() != QStringLiteral("https")) || parsedUrl.host().isEmpty()) return false;

    QVariantMap storedAccount = account;
    QVariantMap previousAccount;
    bool hadExistingAccount = false;
    for (const auto &value : this->caldavAccountsData()) {
        const auto existing = value.toMap();
        if (existing.value(QStringLiteral("url")).toString() != url) continue;
        previousAccount = existing;
        hadExistingAccount = true;
        for (const auto &key : {QStringLiteral("password"), QStringLiteral("refreshToken"), QStringLiteral("clientSecret"), QStringLiteral("tokenUrl"), QStringLiteral("clientId"), QStringLiteral("authorizationUrl"), QStringLiteral("tokenExpiresAt")})
            if (storedAccount.value(key).toString().isEmpty()) storedAccount.insert(key, existing.value(key));
        break;
    }

    const auto authType = storedAccount.value(QStringLiteral("authType"), QStringLiteral("basic")).toString().compare(QStringLiteral("bearer"), Qt::CaseInsensitive) == 0 ? QStringLiteral("bearer") : QStringLiteral("basic");
    const auto username = storedAccount.value(QStringLiteral("username")).toString().trimmed();
    const auto previousUsername = previousAccount.value(QStringLiteral("username")).toString();
    storedAccount.insert(QStringLiteral("username"), username);
    storedAccount.insert(QStringLiteral("authType"), authType);
    if (authType == QStringLiteral("bearer") && storedAccount.value(QStringLiteral("password")).toString().trimmed().isEmpty() && storedAccount.value(QStringLiteral("refreshToken")).toString().trimmed().isEmpty()) return false;
    if (authType != QStringLiteral("bearer")) {
        storedAccount.remove(QStringLiteral("refreshToken"));
        storedAccount.remove(QStringLiteral("tokenUrl"));
        storedAccount.remove(QStringLiteral("clientId"));
        storedAccount.remove(QStringLiteral("clientSecret"));
        storedAccount.remove(QStringLiteral("authorizationUrl"));
        storedAccount.remove(QStringLiteral("tokenExpiresAt"));
    }

    const auto attributes = credentialAttributes(url, username);
    const auto previousAttributes = credentialAttributes(url, previousUsername);
    auto &secretStore = *MauiSecretStore::instance();
    const auto writeSecret = [&secretStore](const QString &kind, const QVariantMap &attributes, const QString &value) {
        return value.isEmpty() ? secretStore.remove(QStringLiteral("mauikit"), kind, attributes) : secretStore.save(QStringLiteral("mauikit"), kind, attributes, value);
    };
    const auto restoreSecret = [&writeSecret](const QString &kind, const QVariantMap &attributes, const QVariantMap &account, const QString &key) {
        return writeSecret(kind, attributes, account.value(key).toString());
    };
    const auto removeSecrets = [&writeSecret](const QVariantMap &attributes) {
        bool result = true;
        result = writeSecret(QStringLiteral("caldav-password"), attributes, QString()) && result;
        result = writeSecret(QStringLiteral("caldav-refresh-token"), attributes, QString()) && result;
        result = writeSecret(QStringLiteral("caldav-client-secret"), attributes, QString()) && result;
        return result;
    };

    bool secretsSaved = writeSecret(QStringLiteral("caldav-password"), attributes, storedAccount.value(QStringLiteral("password")).toString());
    if (authType == QStringLiteral("bearer")) {
        secretsSaved = writeSecret(QStringLiteral("caldav-refresh-token"), attributes, storedAccount.value(QStringLiteral("refreshToken")).toString()) && secretsSaved;
        secretsSaved = writeSecret(QStringLiteral("caldav-client-secret"), attributes, storedAccount.value(QStringLiteral("clientSecret")).toString()) && secretsSaved;
    } else {
        secretsSaved = removeSecrets(attributes) && secretsSaved;
    }
    const auto rollbackSecrets = [&]() {
        if (!hadExistingAccount) {
            removeSecrets(attributes);
            return;
        }
        if (attributes != previousAttributes)
            removeSecrets(attributes);
        restoreSecret(QStringLiteral("caldav-password"), previousAttributes, previousAccount, QStringLiteral("password"));
        restoreSecret(QStringLiteral("caldav-refresh-token"), previousAttributes, previousAccount, QStringLiteral("refreshToken"));
        restoreSecret(QStringLiteral("caldav-client-secret"), previousAttributes, previousAccount, QStringLiteral("clientSecret"));
    };

    if (!secretsSaved) {
        rollbackSecrets();
        return false;
    }

    auto query = this->db->getQuery(QString());
    query.prepare(QStringLiteral("INSERT OR REPLACE INTO caldav (url, username, authType, tokenUrl, clientId, authorizationUrl, tokenExpiresAt, enabled) VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));
    query.addBindValue(url);
    query.addBindValue(username);
    query.addBindValue(authType);
    query.addBindValue(storedAccount.value(QStringLiteral("tokenUrl")).toString());
    query.addBindValue(storedAccount.value(QStringLiteral("clientId")).toString());
    query.addBindValue(storedAccount.value(QStringLiteral("authorizationUrl")).toString());
    const auto tokenExpiresAt = storedAccount.value(QStringLiteral("tokenExpiresAt"));
    query.addBindValue(tokenExpiresAt.canConvert<QDateTime>() ? tokenExpiresAt.toDateTime().toString(Qt::ISODate) : tokenExpiresAt.toString());
    query.addBindValue(storedAccount.value(QStringLiteral("enabled"), true).toBool() ? 1 : 0);

    if (!query.exec()) {
        qWarning() << "Could not save CalDAV account:" << query.lastError().text();
        rollbackSecrets();
        return false;
    }

    if (hadExistingAccount && previousUsername != username)
        removeSecrets(previousAttributes);

    Q_EMIT this->caldavAccountsChanged();
    return true;
}

bool MauiAccounts::removeCalDavAccount(const QString &url)
{
    QString username;
    for (const auto &value : this->caldavAccountsData()) {
        const auto existing = value.toMap();
        if (existing.value(QStringLiteral("url")).toString() == url.trimmed()) {
            username = existing.value(QStringLiteral("username")).toString();
            break;
        }
    }

    auto query = this->db->getQuery(QString());
    query.prepare(QStringLiteral("DELETE FROM caldav WHERE url = ?"));
    query.addBindValue(url.trimmed());
    if (!query.exec()) {
        qWarning() << "Could not remove CalDAV account:" << query.lastError().text();
        return false;
    }

    const auto removed = query.numRowsAffected() > 0;
    if (removed) {
        const auto attributes = credentialAttributes(url.trimmed(), username);
        MauiSecretStore::instance()->remove(QStringLiteral("mauikit"), QStringLiteral("caldav-password"), attributes);
        MauiSecretStore::instance()->remove(QStringLiteral("mauikit"), QStringLiteral("caldav-refresh-token"), attributes);
        MauiSecretStore::instance()->remove(QStringLiteral("mauikit"), QStringLiteral("caldav-client-secret"), attributes);
        Q_EMIT this->caldavAccountsChanged();
    }
    return removed;
}

bool MauiAccounts::addCloudAccount(const QString &server, const QString &user, const QString &password)
{
    const QVariantMap account = {{FMH::MODEL_NAME[FMH::MODEL_KEY::SERVER], server}, {FMH::MODEL_NAME[FMH::MODEL_KEY::USER], user}, {FMH::MODEL_NAME[FMH::MODEL_KEY::PASSWORD], password}};
    if (!this->db->insert("cloud", {{QStringLiteral("server"), server}, {QStringLiteral("user"), user}}))
        return false;
    if (!MauiSecretStore::instance()->save(QStringLiteral("mauikit"), QStringLiteral("cloud"), credentialAttributes(server, user), password)) {
        this->db->remove("cloud", FMH::MODEL{{FMH::MODEL_KEY::SERVER, server}, {FMH::MODEL_KEY::USER, user}});
        return false;
    }
    Q_EMIT this->accountAdded(account);
    return true;
}

bool MauiAccounts::removeCloudAccount(const QString &server, const QString &user)
{
    FMH::MODEL account = {
        {FMH::MODEL_KEY::SERVER, server},
        {FMH::MODEL_KEY::USER, user},
    };

    if (this->db->remove("cloud", account)) {
        MauiSecretStore::instance()->remove(QStringLiteral("mauikit"), QStringLiteral("cloud"), credentialAttributes(server, user));
        Q_EMIT this->accountRemoved(FMH::toMap(account));
        return true;
    }

    return false;
}

QVariantList MauiAccounts::get(const QString &queryTxt)
{
    QVariantList mapList;

    auto query = this->db->getQuery(queryTxt);

    if (query.exec()) {
        const auto keys = FMH::MODEL_NAME.keys();

        while (query.next()) {
            QVariantMap data;
            for (auto key : keys)
                if (query.record().indexOf(FMH::MODEL_NAME[key]) > -1)
                    data[FMH::MODEL_NAME[key]] = query.value(FMH::MODEL_NAME[key]).toString();

            mapList << data;
        }

    } else
        qDebug() << query.lastError();

    return mapList;
}

int MauiAccounts::getCurrentAccountIndex() const
{
    return this->m_currentAccountIndex;
}

QVariantMap MauiAccounts::getCurrentAccount() const
{
    return this->m_currentAccount;
}

void MauiAccounts::registerAccount(const QVariantMap &account)
{
    // register the account to the backend needed
    auto model = FMH::toModel(account);

    if (this->addCloudAccount(model[FMH::MODEL_KEY::SERVER], model[FMH::MODEL_KEY::USER], model[FMH::MODEL_KEY::PASSWORD])) {
        this->setAccounts();
    }
}

void MauiAccounts::setCurrentAccountIndex(const int &index)
{
    if (index >= this->m_data.size() || index < 0)
        return;

    if (index == this->m_currentAccountIndex)
        return;

    // make sure the account exists
    this->m_currentAccountIndex = index;
    this->m_currentAccount = FMH::toMap(this->m_data.at(m_currentAccountIndex));

    Q_EMIT this->currentAccountChanged(this->m_currentAccount);
    Q_EMIT this->currentAccountIndexChanged(this->m_currentAccountIndex);
}

QVariantList MauiAccounts::getCloudAccountsList()
{
    QVariantList res;

    const auto data = this->getCloudAccounts();
    for (const auto &item : data)
        res << FMH::toMap(item);

    return res;
}

void MauiAccounts::refresh()
{
    this->setAccounts();
}

void MauiAccounts::removeAccount(const int &index)
{
    if (index >= this->m_data.size() || index < 0)
        return;

    if (this->removeCloudAccount(this->m_data.at(index)[FMH::MODEL_KEY::SERVER], this->m_data.at(index)[FMH::MODEL_KEY::USER])) {
        this->refresh();
    }
}

void MauiAccounts::removeAccountAndFiles(const int &index)
{
    if (index >= this->m_data.size() || index < 0)
        return;

    if (this->removeCloudAccount(this->m_data.at(index)[FMH::MODEL_KEY::SERVER], this->m_data.at(index)[FMH::MODEL_KEY::USER])) {
        this->refresh();
    }

    //    FM_STATIC::removeDir(FM::resolveUserCloudCachePath(this->m_data.at(index)[FMH::MODEL_KEY::SERVER], this->m_data.at(index)[FMH::MODEL_KEY::USER]));
}
