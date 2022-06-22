#pragma once

#include <bcos-framework/concepts/Storage.h>
#include <boost/throw_exception.hpp>

namespace bcos::storage
{

template <class Storage>
class StorageWrapper
{
public:
    StorageWrapper(Storage storage) : m_storage(std::move(storage)) {}
    StorageWrapper(const StorageWrapper&) = default;
    StorageWrapper(StorageWrapper&&) = default;
    StorageWrapper& operator=(const StorageWrapper&) = default;
    StorageWrapper& operator=(StorageWrapper&&) = default;
    ~StorageWrapper() = default;

    std::optional<Entry> getRow(std::string_view table, std::string_view key)
    {
        Error::UniquePtr error;
        std::optional<storage::Entry> entry;

        m_storage.asyncGetRow(table, key, [&error, &entry](Error::UniquePtr errorOut, std::optional<Entry> entryOut) {
            error = std::move(errorOut);
            entry = std::move(entryOut);
        });

        if (error) BOOST_THROW_EXCEPTION(*error);

        return entry;
    }

    std::vector<std::optional<Entry>> getRows(std::string_view table, std::ranges::range auto&& keys)
    {
        Error::UniquePtr error;
        std::vector<std::optional<Entry>> entries;

        m_storage.asyncGetRows(table, std::forward<decltype(keys)>,
            [&error, &entries](Error::UniquePtr errorOut, std::vector<std::optional<Entry>> entriesOut) {
                error = std::move(errorOut);
                entries = std::move(entriesOut);
            });

        if (error) BOOST_THROW_EXCEPTION(*error);

        return entries;
    }

    void setRow(std::string_view table, std::string_view key, Entry entry)
    {
        Error::UniquePtr error;

        m_storage.asyncSetRow(
            table, key, std::move(entry), [&error](Error::UniquePtr errorOut) { error = std::move(errorOut); });

        if (error) BOOST_THROW_EXCEPTION(*error);
    }

    void createTable(std::string tableName)
    {
        Error::UniquePtr error;

        m_storage->asyncCreateTable(std::move(tableName), std::string{},
            [&error](Error::UniquePtr errorOut, [[maybe_unused]] auto&& table) { error = std::move(errorOut); });
        if (error) BOOST_THROW_EXCEPTION(*error);
    };

private:
    Storage m_storage;
};

static_assert(Storage<StorageWrapper<int>>, "fail!");
}  // namespace bcos::storage