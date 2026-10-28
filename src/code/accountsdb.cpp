#include "accountsdb.h"

#include <QFile>
#include <QRegularExpression>
#include <QUuid>
#include <QStandardPaths>

namespace
{
bool validIdentifier(const QString &identifier)
{
    return QRegularExpression(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$")).match(identifier).hasMatch();
}
}

#ifdef Q_OS_ANDROID
#include "mauiandroid.h"
#endif

const static QUrl DBDir = QUrl::fromLocalFile(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/maui/");
const static QString DBName = "accounts.db";
const static QUrl DBPath = QUrl(DBDir.toString()+"/"+DBName);

AccountsDB::AccountsDB(QObject *parent)
    : QObject(parent)
{
    // get permissions to read and write
#ifdef Q_OS_ANDROID
    MAUIAndroid::checkRunTimePermissions({"android.permission.WRITE_EXTERNAL_STORAGE"});
#endif

    QDir collectionDBPath_dir(DBDir.toLocalFile());
    if (!collectionDBPath_dir.exists())
        collectionDBPath_dir.mkpath(".");

    this->name = QUuid::createUuid().toString();
    this->openDB(this->name);
    this->prepareCollectionDB();
}

AccountsDB::~AccountsDB()
{
    this->m_db.close();
}

void AccountsDB::openDB(const QString &name)
{
    if (!QSqlDatabase::contains(name)) {
        this->m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
        this->m_db.setDatabaseName(DBPath.toLocalFile());
    }

    if (!this->m_db.isOpen()) {
        if (!this->m_db.open())
            qDebug() << "ERROR OPENING DB" << this->m_db.lastError().text() << m_db.connectionName();
    }

    if (this->m_db.isOpen())
        QFile::setPermissions(DBPath.toLocalFile(), QFile::ReadOwner | QFile::WriteOwner);

    auto query = this->getQuery("PRAGMA synchronous=OFF");
    query.exec();
}

void AccountsDB::prepareCollectionDB() const
{
    QSqlQuery query(this->m_db);

    QFile file(":/accounts/script.sql");

    if (!file.exists()) {
        QString log = QStringLiteral("Fatal error on build database. The file '");
        log.append(file.fileName() + QStringLiteral("' for database and tables creation query cannot be not found!"));
        qDebug() << log;
        return;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << QStringLiteral("Fatal error on try to create database! The file with sql queries for database creation cannot be opened!");
        return;
    }

    bool hasText;
    QString line;
    QByteArray readLine;
    QString cleanedLine;
    QStringList strings;

    while (!file.atEnd()) {
        hasText = false;
        line = "";
        readLine = "";
        cleanedLine = "";
        strings.clear();
        while (!hasText) {
            readLine = file.readLine();
            cleanedLine = readLine.trimmed();
            strings = cleanedLine.split("--");
            cleanedLine = strings.at(0);
            if (!cleanedLine.startsWith("--") && !cleanedLine.startsWith("DROP") && !cleanedLine.isEmpty())
                line += cleanedLine;
            if (cleanedLine.endsWith(";"))
                break;
            if (cleanedLine.startsWith("COMMIT"))
                hasText = true;
        }
        if (!line.isEmpty()) {
            if (!query.exec(line)) {
                qDebug() << "exec failed" << query.lastQuery() << query.lastError();
            }

        } else
            qDebug() << "exec wrong" << query.lastError();
    }
    file.close();
}

bool AccountsDB::checkExistance(const QString &tableName, const QString &searchId, const QString &search)
{
    if (!validIdentifier(tableName) || !validIdentifier(searchId))
        return false;

    auto query = this->getQuery(QStringLiteral("SELECT %1 FROM %2 WHERE %1 = ?").arg(searchId, tableName));
    query.addBindValue(search);

    if (query.exec()) {
        if (query.next())
            return true;
    } else
        qDebug() << query.lastError().text();

    return false;
}

bool AccountsDB::checkExistance(const QString &queryStr)
{
    auto query = this->getQuery(queryStr);

    if (query.exec()) {
        if (query.next())
            return true;
    } else
        qDebug() << query.lastError().text();

    return false;
}

QSqlQuery AccountsDB::getQuery(const QString &queryTxt)
{
    QSqlQuery query(queryTxt, this->m_db);
    return query;
}

bool AccountsDB::insert(const QString &tableName, const QVariantMap &insertData)
{
    if (!validIdentifier(tableName)) {
        qDebug() << QStringLiteral("Fatal error on insert! The table name is invalid!");
        return false;
    } else if (insertData.isEmpty()) {
        qDebug() << QStringLiteral("Fatal error on insert! The insertData is empty!");
        return false;
    }

    QStringList strValues;
    QStringList fields = insertData.keys();
    for (const auto &field : fields)
        if (!validIdentifier(field))
            return false;

    QVariantList values = insertData.values();
    int totalFields = fields.size();
    for (int i = 0; i < totalFields; ++i)
        strValues.append(QStringLiteral("?"));

    QString sqlQueryString = QStringLiteral("INSERT INTO %1 (%2) VALUES(%3)").arg(tableName, fields.join(QStringLiteral(",")), strValues.join(QStringLiteral(",")));
    QSqlQuery query(this->m_db);
    query.prepare(sqlQueryString);

    int k = 0;
    for (const QVariant &value : values)
        query.bindValue(k++, value);

    return query.exec();
}

bool AccountsDB::update(const QString &tableName, const FMH::MODEL &updateData, const QVariantMap &where)
{
    if (!validIdentifier(tableName)) {
        qDebug() << QStringLiteral("Fatal error on update! The table name is invalid!");
        return false;
    } else if (updateData.isEmpty() || where.isEmpty()) {
        qDebug() << QStringLiteral("Fatal error on update! The update data or condition is empty!");
        return false;
    }

    QStringList set;
    const auto updateKeys = updateData.keys();
    for (const auto &key : updateKeys) {
        const auto field = FMH::MODEL_NAME[key];
        if (!validIdentifier(field))
            return false;
        set.append(field + QStringLiteral(" = ?"));
    }

    QStringList condition;
    const auto keys = where.keys();
    for (const auto &key : keys) {
        if (!validIdentifier(key))
            return false;
        condition.append(key + QStringLiteral(" = ?"));
    }

    auto query = this->getQuery(QStringLiteral("UPDATE %1 SET %2 WHERE %3").arg(tableName, set.join(QStringLiteral(",")), condition.join(QStringLiteral(" AND "))));
    for (const auto &key : updateKeys)
        query.addBindValue(updateData[key]);
    for (const auto &key : keys)
        query.addBindValue(where[key]);

    return query.exec();
}

bool AccountsDB::update(const QString &table, const QString &column, const QVariant &newValue, const QVariant &op, const QString &id)
{
    if (!validIdentifier(table) || !validIdentifier(column) || !validIdentifier(op.toString()))
        return false;

    auto query = this->getQuery(QStringLiteral("UPDATE %1 SET %2 = ? WHERE %3 = ?").arg(table, column, op.toString()));
    query.addBindValue(newValue);
    query.addBindValue(id);
    return query.exec();
}

bool AccountsDB::remove(const QString &tableName, const FMH::MODEL &removeData)
{
    if (!validIdentifier(tableName)) {
        qDebug() << QStringLiteral("Fatal error on removing! The table name is invalid!");
        return false;
    } else if (removeData.isEmpty()) {
        qDebug() << QStringLiteral("Fatal error on removing! The removeData is empty!");
        return false;
    }

    QStringList conditions;
    const auto keys = removeData.keys();
    for (const auto &key : keys) {
        const auto field = FMH::MODEL_NAME[key];
        if (!validIdentifier(field))
            return false;
        conditions.append(field + QStringLiteral(" = ?"));
    }

    auto query = this->getQuery(QStringLiteral("DELETE FROM %1 WHERE %2").arg(tableName, conditions.join(QStringLiteral(" AND "))));
    for (const auto &key : keys)
        query.addBindValue(removeData[key]);

    return query.exec();
}
