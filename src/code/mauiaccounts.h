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

#pragma once
#include <QObject>

#include <MauiKit4/Core/mauilist.h>

#include "accounts_export.h"

class AccountsDB;

/**
 * @brief MauiAccounts class provides an interface to access the system registered accounts, the current active account and a bridge to add account data to the host system backend solution.
 * This properties and functions are exposed to all the Maui Applications that might want to use the accounts with the `Accounts` property.
 */
class ACCOUNTS_EXPORT MauiAccounts : public MauiList
{
    Q_OBJECT
    /** The selected account row, or -1 when no account is selected. */
    Q_PROPERTY(int currentAccountIndex READ getCurrentAccountIndex WRITE setCurrentAccountIndex NOTIFY currentAccountIndexChanged)

    /** The data for currentAccountIndex, or an empty map when none is selected. */
    Q_PROPERTY(QVariantMap currentAccount READ getCurrentAccount NOTIFY currentAccountChanged)

public:
    static MauiAccounts *instance()
    {
        if (m_instance)
            return m_instance;

        m_instance = new MauiAccounts;
        return m_instance;
    }

    MauiAccounts(const MauiAccounts &) = delete;
    MauiAccounts &operator=(const MauiAccounts &) = delete;
    MauiAccounts(MauiAccounts &&) = delete;
    MauiAccounts &operator=(MauiAccounts &&) = delete;

    const FMH::MODEL_LIST &items() const final override;

    /** Selects the account at  index; out-of-range indexes are ignored. */
    void setCurrentAccountIndex(const int &index);

    /** Returns the selected account row, or -1 when none is selected. */
    int getCurrentAccountIndex() const;

    /** Returns the selected account as a key-value map. */
    QVariantMap getCurrentAccount() const;

    /** Returns the number of registered cloud accounts. */
    uint getCount() const;

    /** Returns the account at  index, or an empty map if it is invalid. */
    QVariantMap get(const int &index) const;

public Q_SLOTS:
    /** Returns all cloud accounts as QVariantMap entries suitable for QML. */
    QVariantList getCloudAccountsList();

    /** Returns all cloud accounts in the native FMH model representation. */
    FMH::MODEL_LIST getCloudAccounts();

    /** Registers  account and refreshes the exposed account list on success. */
    void registerAccount(const QVariantMap &account);

    /** Removes the account at  index and refreshes the account list. */
    void removeAccount(const int &index);

    /**
     * Removes the account at  index.
     *
     *  Cached-file removal is not currently implemented, so this behaves
     * like removeAccount().
     */
    void removeAccountAndFiles(const int &index);

    /** Reloads the account list from persistent storage. */
    void refresh();

private:
    MauiAccounts();
    ~MauiAccounts();

    static MauiAccounts *m_instance;

    AccountsDB *db;
    FMH::MODEL_LIST m_data;
    QVariantMap m_currentAccount;

    int m_currentAccountIndex = -1;
    void setAccounts();

    bool addCloudAccount(const QString &server, const QString &user, const QString &password);
    bool removeCloudAccount(const QString &server, const QString &user);

    QVariantList get(const QString &queryTxt);

Q_SIGNALS:
    /** Emitted after  account is added to persistent storage. */
    void accountAdded(QVariantMap account);

    /** Emitted after  account is removed from persistent storage. */
    void accountRemoved(QVariantMap account);

    /** Emitted when the selected  account changes. */
    void currentAccountChanged(QVariantMap account);

    /** Emitted when the selected account  index changes. */
    void currentAccountIndexChanged(int index);
};
