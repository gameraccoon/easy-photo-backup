// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "server_shared/server_storage.h"

#include "common_shared/bstorage/storage.h"

namespace ServerStorageInternal
{
	static constexpr uint16_t ServerStorageVersion = 0;
	static constexpr std::string_view ServerStoragePath = "./server_storage.bin";
	static constexpr std::string_view ConfirmedField = "confirmed";
	static constexpr std::string_view ConnectionIdField = "conn_id";
	static constexpr std::string_view NameField = "name";
	static constexpr std::string_view RemoteStaticKeyField = "rs";
	static constexpr std::string_view StaticPublicKeyField = "s_pub";
	static constexpr std::string_view StaticSecretKeyField = "s_secret";
	static constexpr std::string_view ServerIdField = "server_id";

	template<size_t N>
	static void tryConsumeObjectFieldArray(BStorage::Value::ObjectMap& record, const std::string_view name, std::array<std::byte, N>& result)
	{
		if (auto it = record.find(name); it != record.end())
		{
			if (std::vector<std::byte>* v = it->second.asByteArray())
			{
				if (v->size() == N)
				{
					std::copy(v->begin(), v->end(), result.begin());
				}
			}
		}
	}

	template<typename T>
	static void tryConsumeObjectField(BStorage::Value::ObjectMap& record, const std::string_view name, T& result)
	{
		constexpr auto getT = [](BStorage::Value& v) -> T* {
			if constexpr (std::is_same_v<T, std::string>)
			{
				return v.asString();
			}
			else if constexpr (std::is_same_v<T, std::vector<std::byte>>)
			{
				return v.asByteArray();
			}
			else if constexpr (std::is_same_v<T, uint64_t>)
			{
				return v.asU64();
			}
			else
			{
				static_assert(false, "Unknown type");
			}
		};

		if (auto it = record.find(name); it != record.end())
		{
			if (T* v = getT(it->second))
			{
				result = std::move(*v);
			}
		}
	}

	static BStorage::Value WriteConfirmedClientBindingsToValue(const ServerStorageData::ConfirmedClientBindingsType& confirmedClientBindings)
	{
		std::vector<BStorage::Value> vec;
		vec.reserve(confirmedClientBindings.size());
		for (auto& pair : confirmedClientBindings)
		{
			BStorage::Value::ObjectMap record;
			record.reserve(4);
			record.emplace(ConnectionIdField, BStorage::Value::makeByteArray(std::vector<std::byte>(pair.first.raw.begin(), pair.first.raw.end())));
			record.emplace(NameField, BStorage::Value::makeString(pair.second.name));
			record.emplace(StaticPublicKeyField, BStorage::Value::makeByteArray(std::vector<std::byte>(pair.second.staticKeys.publicKey.raw.begin(), pair.second.staticKeys.publicKey.raw.end())));
			record.emplace(StaticSecretKeyField, BStorage::Value::makeByteArray(std::vector<std::byte>(pair.second.staticKeys.secretKey.raw.begin(), pair.second.staticKeys.secretKey.raw.end())));
			record.emplace(RemoteStaticKeyField, BStorage::Value::makeByteArray(std::vector<std::byte>(pair.second.remoteStaticKey.raw.begin(), pair.second.remoteStaticKey.raw.end())));
			vec.push_back(BStorage::Value::makeObject(std::move(record)));
		}

		return BStorage::Value::makeArray(std::move(vec));
	}

	static void ReadConfirmedClientBindingsToValue(BStorage::Value&& value, ServerStorageData::ConfirmedClientBindingsType& confirmedClientBindings)
	{
		if (std::vector<BStorage::Value>* vec = value.asArray())
		{
			confirmedClientBindings.reserve(vec->size());
			for (BStorage::Value& val : *vec)
			{
				if (BStorage::Value::ObjectMap* record = val.asObject())
				{
					ServerStorageData::ClientBinding newItem{};
					Cryptography::HashResult id;
					tryConsumeObjectFieldArray(*record, ConnectionIdField, id.raw);
					tryConsumeObjectField<std::string>(*record, NameField, newItem.name);
					tryConsumeObjectFieldArray(*record, StaticPublicKeyField, newItem.staticKeys.publicKey.raw);
					tryConsumeObjectFieldArray(*record, StaticSecretKeyField, newItem.staticKeys.secretKey.raw);
					tryConsumeObjectFieldArray(*record, RemoteStaticKeyField, newItem.remoteStaticKey.raw);
					confirmedClientBindings.emplace(std::move(id), std::move(newItem));
				}
			}
		}
	}

	static BStorage::Value WriteServerStorageDataToValue(const ServerStorageData& data)
	{
		BStorage::Value::ObjectMap clientStorageDataObject;
		clientStorageDataObject.reserve(2);
		clientStorageDataObject.emplace(
			ConfirmedField,
			WriteConfirmedClientBindingsToValue(data.confirmedClientBindings)
		);
		clientStorageDataObject.emplace(ServerIdField, BStorage::Value::makeByteArray(data.serverId));
		return BStorage::Value::makeObject(std::move(clientStorageDataObject));
	}

	static ServerStorageData ReadServerStorageDataFromValue(BStorage::Value&& value)
	{
		ServerStorageData result{};
		BStorage::Value::ObjectMap* object = value.asObject();
		if (object != nullptr)
		{
			if (auto it = object->find(ConfirmedField); it != object->end())
			{
				ReadConfirmedClientBindingsToValue(std::move(it->second), result.confirmedClientBindings);
			}

			tryConsumeObjectFieldArray(*object, ServerIdField, result.serverId);
		}
		return result;
	}
} // namespace ServerStorageInternal

ServerStorage ServerStorage::load(const std::filesystem::path& root) noexcept
{
	std::optional<std::tuple<BStorage::Value, uint16_t>> loaded = BStorage::loadStorage(root / ServerStorageInternal::ServerStoragePath);

	if (loaded.has_value())
	{
		if (std::get<1>(*loaded) != ServerStorageInternal::ServerStorageVersion)
		{
			// here we need to have an update path
			return ServerStorage(BStorage::Value::makeObject({}));
		}

		return ServerStorage(std::get<0>(std::move(*loaded)));
	}
	else
	{
		return ServerStorage(BStorage::Value::makeObject({}));
	}
}

bool ServerStorage::save() const noexcept
{
	using namespace ServerStorageInternal;

	std::lock_guard g(mMutex);
	return BStorage::saveStorage(ServerStoragePath, WriteServerStorageDataToValue(mStorageData), ServerStorageVersion);
}

void ServerStorage::read(const std::function<void(const ServerStorageData&)>& readFn) const noexcept
{
	std::lock_guard g(mMutex);
	readFn(mStorageData);
}

void ServerStorage::mutate(const std::function<void(ServerStorageData&)>& mutateFn) noexcept
{
	std::lock_guard g(mMutex);
	mutateFn(mStorageData);
}

ServerStorage::ServerStorage(BStorage::Value&& value) noexcept
	: mStorageData(ServerStorageInternal::ReadServerStorageDataFromValue(std::move(value)))
{
}
