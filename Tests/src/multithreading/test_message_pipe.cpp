// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include <array>
#include <string>

#include <gtest/gtest.h>

#include "common_shared/multithreading/message_pipe.h"

TEST(MessagePipe, MessagePipe_CreatedDestroyed_NoErrors)
{
	Multithreading::MessagePipe<std::string> p1;
	Multithreading::MessagePipe<int> p2;
	Multithreading::MessagePipe<std::array<std::string, 3>> p3;
}

TEST(MessagePipe, MessagePipeWithVectorAsSequenceType_CreatedDestroyed_NoErrors)
{
	Multithreading::MessagePipe<std::string, std::vector<std::string>> p1;
	Multithreading::MessagePipe<int, std::vector<int>> p2;
	Multithreading::MessagePipe<std::array<std::string, 3>, std::vector<std::array<std::string, 3>>> p3;
}

TEST(MessagePipe, MessagePipe_SendAndRecv_GetTheSameValues)
{
	Multithreading::MessagePipe<std::string> pipe;

	pipe.send("test1");
	pipe.send("test2");

	EXPECT_EQ(pipe.recv(std::chrono::milliseconds(500)), std::string("test1"));
	EXPECT_EQ(pipe.recv(std::chrono::milliseconds(500)), std::string("test2"));
}

TEST(MessagePipe, MessagePipe_SendEmplaceAndRecv_GetTheSameValues)
{
	Multithreading::MessagePipe<std::vector<int>> pipe;

	pipe.sendEmplace(1, 2);
	pipe.sendEmplace(2, 3);

	EXPECT_EQ(pipe.recv(std::chrono::milliseconds(500)), std::vector(1, 2));
	EXPECT_EQ(pipe.recv(std::chrono::milliseconds(500)), std::vector(2, 3));
}

TEST(MessagePipe, EmptyMessagePipe_RecvWithEmptyTimeout_GetNullopt)
{
	Multithreading::MessagePipe<std::string> pipe;

	EXPECT_EQ(pipe.recv(std::chrono::seconds(0)), std::nullopt);
	EXPECT_EQ(pipe.recv(std::chrono::seconds(0)), std::nullopt);
}

TEST(MessagePipe, MessagePipe_SendAndImmediatelyRecvWithEmptyTimeout_GetTheSameValues)
{
	Multithreading::MessagePipe<std::string> pipe;

	pipe.send("test");
	pipe.send("test1");

	// the check for emptinness should happen before the check for timeout
	EXPECT_EQ(pipe.recv(std::chrono::seconds(0)), std::string("test"));
	EXPECT_EQ(pipe.recv(std::chrono::seconds(0)), std::string("test1"));
}

TEST(MessagePipe, EmptyMessagePipe_RecvWithNonZeroTimeout_TimesOutCorrectly)
{
	Multithreading::MessagePipe<int> pipe;

	auto start = std::chrono::steady_clock::now();
	auto result = pipe.recv(std::chrono::milliseconds(10));
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);

	EXPECT_FALSE(result.has_value());
	EXPECT_GE(duration.count(), 8); // add some allowance
}

TEST(MessagePipe, EmptyMessagePipe_RecvAllCurrent_GetEmptyContainer)
{
	Multithreading::MessagePipe<std::string, std::vector<std::string>> pipe;
	EXPECT_EQ(pipe.recvAllCurrent(), std::vector<std::string>());
}

TEST(MessagePipe, MessagePipe_SendAndRecvAllCurrent_GetTheSameValues)
{
	Multithreading::MessagePipe<std::string, std::vector<std::string>> pipe;

	pipe.send("test");
	pipe.send("test1");

	const std::vector<std::string> expected{ { "test" }, { "test1" } };
	EXPECT_EQ(pipe.recvAllCurrent(), expected);
}

TEST(MessagePipe, ClosedPipe_RecvAllCurrent_ReturnsNullopt)
{
	Multithreading::MessagePipe<std::string, std::vector<std::string>> pipe;
	pipe.close();

	EXPECT_EQ(pipe.recvAllCurrent(), std::nullopt);
	EXPECT_TRUE(pipe.isClosed());
}

TEST(MessagePipe, EmptyMessagePipe_TryRecv_GetNullopt)
{
	Multithreading::MessagePipe<std::string> pipe;

	EXPECT_EQ(pipe.tryRecv(), std::nullopt);
	EXPECT_EQ(pipe.tryRecv(), std::nullopt);
}

TEST(MessagePipe, MessagePipe_SendAndTryRecv_GetTheSameValues)
{
	Multithreading::MessagePipe<std::string> pipe;

	pipe.send("test");
	pipe.send("test1");

	EXPECT_EQ(pipe.tryRecv(), std::string("test"));
	EXPECT_EQ(pipe.tryRecv(), std::string("test1"));
}

TEST(MessagePipe, MessagePipe_SendAndRecvFromDifferentThread_GetTheSameValues)
{
	Multithreading::MessagePipe<std::string> pipe;

	auto t = std::thread([&pipe]() {
		for (int i = 0; i < 100; ++i)
		{
			EXPECT_EQ(std::string("test" + std::to_string(i)), pipe.recv(std::chrono::milliseconds(500)));
		}
	});

	std::this_thread::yield();
	for (int i = 0; i < 100; ++i)
	{
		pipe.send("test" + std::to_string(i));
	}

	t.join();
}

TEST(MessagePipe, MessagePipe_MultipleProducersSingleConsumer_AllMessagesReceived)
{
	Multithreading::MessagePipe<int> pipe;
	constexpr int numProducers = 4;
	constexpr int messagesPerProducer = 250;
	std::atomic<int> receivedCount{ 0 };

	std::vector<std::thread> producers;
	for (int p = 0; p < numProducers; ++p)
	{
		producers.emplace_back([&pipe, p]() {
			for (int i = 0; i < messagesPerProducer; ++i)
			{
				pipe.send(p * 1000 + i);
			}
		});
	}

	std::thread consumer([&pipe, &receivedCount]() {
		while (receivedCount < numProducers * messagesPerProducer)
		{
			auto val = pipe.recv(std::chrono::milliseconds(1000));
			if (val.has_value())
			{
				receivedCount++;
			}
		}
	});

	std::this_thread::yield();
	for (auto& t : producers)
	{
		t.join();
	}
	consumer.join();

	EXPECT_EQ(receivedCount, numProducers * messagesPerProducer);
}

TEST(MessagePipe, ThrowingMovableStruct_TryMatchNothrowMovableDataConcept_DoesNotMatch)
{
	struct ThrowingMovable
	{
		ThrowingMovable(ThrowingMovable&&) noexcept(false) {}
		ThrowingMovable& operator=(ThrowingMovable&&) noexcept(false) { return *this; }
	};

	static_assert(!Multithreading::NothrowMovableData<ThrowingMovable>, "The concept should not have been instanciated");
}

TEST(MessagePipe, MessagePipeAndOneWaitingReader_ClosePipe_ReaderIsUnblocked)
{
	Multithreading::MessagePipe<std::string> pipe;

	auto t = std::thread([&pipe]() {
		auto start = std::chrono::steady_clock::now();
		auto result = pipe.recv(std::chrono::seconds(2));
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);

		EXPECT_FALSE(result.has_value());
		EXPECT_TRUE(pipe.isClosed());
		EXPECT_LT(duration.count(), 20) << "We waited too long before finding out that the pipe is closed";
	});

	std::this_thread::yield();
	pipe.close();

	t.join();
}

TEST(MessagePipe, MessagePipeWithData_ClosePipeAndRecv_DrainsRemainingMessagesBeforeReturningNullopt)
{
	Multithreading::MessagePipe<std::string> pipe;
	pipe.send("test1");
	pipe.send("test2");

	pipe.close();

	EXPECT_EQ(pipe.recv(std::chrono::seconds(0)), std::string("test1"));
	EXPECT_EQ(pipe.recv(std::chrono::seconds(0)), std::string("test2"));
	EXPECT_EQ(pipe.recv(std::chrono::seconds(0)), std::nullopt);
	EXPECT_TRUE(pipe.isClosed());
}

TEST(MessagePipe, MessagePipeWithData_ClosePipeAndRecvAllCurrent_DrainsRemainingMessagesBeforeReturningNullopt)
{
	Multithreading::MessagePipe<std::string, std::vector<std::string>> pipe;
	pipe.send("test1");
	pipe.send("test2");

	pipe.close();

	const std::vector<std::string> expected{ { { "test1" }, { "test2" } } };
	EXPECT_EQ(pipe.recvAllCurrent(), expected);
	EXPECT_EQ(pipe.recvAllCurrent(), std::nullopt);
	EXPECT_TRUE(pipe.isClosed());
}

TEST(MessagePipe, MessagePipe_ClosePipeAndSend_NewMesssagesIgnored)
{
	Multithreading::MessagePipe<std::string> pipe;

	pipe.close();
	pipe.send("should be ignored");

	EXPECT_EQ(pipe.tryRecv(), std::nullopt);
	EXPECT_TRUE(pipe.isClosed());
}
