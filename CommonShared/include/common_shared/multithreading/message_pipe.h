// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <type_traits>

namespace Multithreading
{
	template<typename T>
	concept NothrowMovableData = std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_assignable_v<T>;

	// Simple pipe with one reader and one writer
	template<NothrowMovableData T, typename QueueType = std::deque<T>>
	class MessagePipe
	{
	public:
		MessagePipe() = default;

		MessagePipe(const MessagePipe&) = delete;
		MessagePipe& operator=(const MessagePipe&) = delete;
		MessagePipe(MessagePipe&&) noexcept = delete;
		MessagePipe& operator=(MessagePipe&&) noexcept = delete;
		~MessagePipe() = default;

		template<typename Tt>
		void send(Tt&& data)
		{
			std::lock_guard<std::mutex> lock(mDataMutex);
			if (mIsPipeClosed) [[unlikely]]
			{
				return;
			}
			mQueue.push_back(std::forward<Tt>(data));
			mCondVar.notify_one();
		}

		template<typename... Args>
		void sendEmplace(Args&&... args)
		{
			std::lock_guard<std::mutex> lock(mDataMutex);
			if (mIsPipeClosed) [[unlikely]]
			{
				return;
			}
			mQueue.emplace_back(std::forward<Args>(args)...);
			mCondVar.notify_one();
		}

		// blocking, returns std::nullopt on timeout or if the pipe is closed
		std::optional<T> recv(std::chrono::steady_clock::duration timeout) noexcept
		{
			std::unique_lock<std::mutex> lock(mDataMutex);
			if (!mCondVar.wait_for(lock, timeout, [&] {
					return !mQueue.empty() || mIsPipeClosed;
				})) [[unlikely]]
			{
				// timeout
				return std::nullopt;
			}

			if (mIsPipeClosed && mQueue.empty()) [[unlikely]]
			{
				return std::nullopt;
			}

			T val = std::move(mQueue.front());
			mQueue.pop_front();
			return val;
		}

		// returns std::nullopt if the pipe is closed
		std::optional<QueueType> recvAllCurrent() noexcept
		{
			std::unique_lock<std::mutex> lock(mDataMutex);
			if (mIsPipeClosed && mQueue.empty())
			{
				return std::nullopt;
			}
			return std::move(mQueue);
		}

		// returns std::nullopt if there is nothing to consume (including, when the pipe is closed)
		std::optional<T> tryRecv() noexcept
		{
			std::unique_lock<std::mutex> lock(mDataMutex);
			if (mQueue.empty())
			{
				return std::nullopt;
			}
			T val = std::move(mQueue.front());
			mQueue.pop_front();
			return val;
		}

		void close() noexcept
		{
			std::lock_guard<std::mutex> lock(mDataMutex);
			mIsPipeClosed = true;
			mCondVar.notify_one();
		}

		bool isClosed() noexcept
		{
			std::unique_lock<std::mutex> lock(mDataMutex);
			return mIsPipeClosed;
		}

	private:
		std::mutex mDataMutex;
		std::condition_variable mCondVar;
		QueueType mQueue;
		bool mIsPipeClosed = false;
	};
} // namespace Multithreading
