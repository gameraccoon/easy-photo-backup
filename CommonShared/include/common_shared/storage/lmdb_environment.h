// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <filesystem>

#include "common_shared/storage/lmdb_return_codes.h"

struct MDB_env;

namespace Lmdb
{
	class Environment
	{
	public:
		virtual ~Environment() noexcept;

		Environment(const Environment&) = delete;
		Environment& operator=(const Environment&) = delete;
		Environment(Environment&&) noexcept;
		Environment& operator=(Environment&&) noexcept;

		[[nodiscard]] Result<int> checkForStaleReaders() noexcept;

		[[nodiscard]] bool isValid() const noexcept { return mMdbEnvironment != nullptr; }
		[[nodiscard]] MDB_env* getRaw() noexcept { return mMdbEnvironment; };

	protected:
		Environment(MDB_env* mdbEnvironment) noexcept;
		// hack to ensure Environment can't be instantiated by itself
		virtual void makeMeAbstract() const noexcept = 0;

	protected:
		MDB_env* mMdbEnvironment;
	};

	class ReadOnlyEnvironment : public Environment
	{
	public:
		[[nodiscard]] static Result<ReadOnlyEnvironment> open(const std::filesystem::path& path, size_t maxNamedDatabases) noexcept;

	protected:
		void makeMeAbstract() const noexcept override {}

	private:
		ReadOnlyEnvironment(MDB_env* mdbEnvironment) noexcept;
	};

	class ReadWriteEnvironment : public Environment
	{
	public:
		~ReadWriteEnvironment() noexcept;
		ReadWriteEnvironment(const ReadWriteEnvironment&) = delete;
		ReadWriteEnvironment& operator=(const ReadWriteEnvironment&) = delete;
		ReadWriteEnvironment(ReadWriteEnvironment&&) noexcept = default;
		ReadWriteEnvironment& operator=(ReadWriteEnvironment&&) noexcept = default;

		[[nodiscard]] static Result<ReadWriteEnvironment> open(const std::filesystem::path& path, size_t maxNamedDatabases) noexcept;

	protected:
		void makeMeAbstract() const noexcept override {}

	private:
		ReadWriteEnvironment(MDB_env* mdbEnvironment) noexcept;
	};
} // namespace Lmdb
