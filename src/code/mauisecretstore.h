/*
 * Copyright (C) 2026 Nitrux Latinoamericana S.C.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 2.1 or version 3.
 */

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include "accounts_export.h"

/**
 * @brief Shared Secret Service storage for MauiKit applications.
 *
 * Account metadata belongs in the application or MauiKit account database;
 * passwords, tokens, and other secret values belong here.
 */
class ACCOUNTS_EXPORT MauiSecretStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)

public:
    static MauiSecretStore *instance();

    bool available() const;

    /** Stores a secret using the shared MauiKit schema. */
    Q_INVOKABLE bool save(const QString &application,
                          const QString &kind,
                          const QVariantMap &attributes,
                          const QString &secret,
                          const QString &label = QString());

    /** Returns found, secret, and matching attributes for one credential. */
    Q_INVOKABLE QVariantMap find(const QString &application,
                                 const QString &kind,
                                 const QVariantMap &attributes) const;

    /** Returns all matching credentials, including their secret values. */
    Q_INVOKABLE QVariantList findAll(const QString &application,
                                     const QString &kind,
                                     const QVariantMap &attributes = QVariantMap()) const;

    /** Removes credentials matching the shared schema attributes. */
    Q_INVOKABLE bool remove(const QString &application,
                            const QString &kind,
                            const QVariantMap &attributes);

    /**
     * Imports entries from an application-specific legacy libsecret schema,
     * copying its attributes into the shared MauiKit schema.
     */
    bool migrateLegacy(const QString &legacySchema,
                       const QStringList &legacyAttributes,
                       const QString &application,
                       const QString &kind);

private:
    explicit MauiSecretStore(QObject *parent = nullptr);
    ~MauiSecretStore() override = default;

    QVariantList findLegacy(const QString &schemaName,
                            const QStringList &attributeNames) const;
    bool removeLegacy(const QString &schemaName,
                      const QStringList &attributeNames,
                      const QVariantMap &attributes) const;
};
