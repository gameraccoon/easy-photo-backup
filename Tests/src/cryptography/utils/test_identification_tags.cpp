// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "tests/random_helpers.h"
#include <gtest/gtest.h>

#include "common_shared/cryptography/utils/identification_tags.h"

TEST(IdentificationTags, EmptyTagMap_Populate_ContainsTagsForThreePeriods)
{
	constexpr size_t PeriodSize = 16;
	Cryptography::IdentificationTagMap<PeriodSize> tagMap;

	std::vector<Cryptography::HashResult> clients;
	Cryptography::HashResult clientHash;
	fillWithTestRandomBytes(clientHash, getSessionRandomSeed());
	clients.push_back(clientHash.clone());
	tagMap.resetMap(clients, 100);

	int count = 0;
	tagMap.forEachPayload([&count](auto /*tag*/) {
		++count;
	});
	EXPECT_EQ(16 * 3, count);
}

TEST(IdentificationTags, PopulatedTagMap_ConsumeTags_AllTagsFound)
{
	constexpr size_t PeriodSize = 16;
	Cryptography::IdentificationTagMap<PeriodSize> tagMap;
	std::vector<Cryptography::HashResult> clients;
	Cryptography::HashResult clientHash;
	fillWithTestRandomBytes(clientHash, getSessionRandomSeed());
	clients.push_back(clientHash.clone());
	tagMap.resetMap(clients, 120);

	for (uint32_t periodIndex = 119; periodIndex <= 121; ++periodIndex)
	{
		for (uint32_t indexInPeriod = 0; indexInPeriod < PeriodSize; ++indexInPeriod)
		{
			auto value = tagMap.consumeTag(Cryptography::getSingleIdentificationTag(clientHash, periodIndex, indexInPeriod));
			ASSERT_TRUE(value.has_value());
			EXPECT_EQ(value->clientIndex, static_cast<uint32_t>(0));
			EXPECT_EQ(value->periodIndex, periodIndex);
			EXPECT_EQ(value->keyIndexInPeriod, indexInPeriod);
		}
	}
}

TEST(IdentificationTags, PopulatedTagMap_ConsumeTagsTwice_OnlyFirstConsumed)
{
	constexpr size_t PeriodSize = 16;
	Cryptography::IdentificationTagMap<PeriodSize> tagMap;
	std::vector<Cryptography::HashResult> clients;
	Cryptography::HashResult clientHash;
	fillWithTestRandomBytes(clientHash, getSessionRandomSeed());
	clients.push_back(clientHash.clone());
	tagMap.resetMap(clients, 140);

	EXPECT_TRUE(tagMap.consumeTag(Cryptography::getSingleIdentificationTag(clientHash, 140, 3)).has_value());
	EXPECT_FALSE(tagMap.consumeTag(Cryptography::getSingleIdentificationTag(clientHash, 140, 3)).has_value());
	EXPECT_TRUE(tagMap.consumeTag(Cryptography::getSingleIdentificationTag(clientHash, 139, 6)).has_value());
	EXPECT_FALSE(tagMap.consumeTag(Cryptography::getSingleIdentificationTag(clientHash, 139, 6)).has_value());
	EXPECT_TRUE(tagMap.consumeTag(Cryptography::getSingleIdentificationTag(clientHash, 141, 10)).has_value());
	EXPECT_FALSE(tagMap.consumeTag(Cryptography::getSingleIdentificationTag(clientHash, 141, 10)).has_value());
}

TEST(IdentificationTags, PopulatedTagMap_ConsumeFromBeyondPopulatedPeriods_NotFound)
{
	constexpr size_t PeriodSize = 16;
	Cryptography::IdentificationTagMap<PeriodSize> tagMap;
	std::vector<Cryptography::HashResult> clients;
	Cryptography::HashResult clientHash;
	fillWithTestRandomBytes(clientHash, getSessionRandomSeed());
	clients.push_back(clientHash.clone());
	tagMap.resetMap(clients, 160);

	EXPECT_FALSE(tagMap.consumeTag(Cryptography::getSingleIdentificationTag(clientHash, 120, 10)).has_value());
	EXPECT_FALSE(tagMap.consumeTag(Cryptography::getSingleIdentificationTag(clientHash, 158, 0)).has_value());
	EXPECT_FALSE(tagMap.consumeTag(Cryptography::getSingleIdentificationTag(clientHash, 162, 4)).has_value());
	EXPECT_FALSE(tagMap.consumeTag(Cryptography::getSingleIdentificationTag(clientHash, 160, 16)).has_value());
}

TEST(IdentificationTags, PopulatedTagMap_MovePeriodForwardByOneAndConsumeAll_OnlyCorrectPeriodsAreConsumed)
{
	constexpr size_t PeriodSize = 16;
	Cryptography::IdentificationTagMap<PeriodSize> tagMap;

	std::vector<Cryptography::HashResult> clients;
	Cryptography::HashResult clientHash;
	fillWithTestRandomBytes(clientHash, getSessionRandomSeed());
	clients.push_back(clientHash.clone());
	tagMap.resetMap(clients, 180);

	tagMap.updateTagMap(clients, 180, 181);

	for (uint32_t indexInPeriod = 0; indexInPeriod < PeriodSize; ++indexInPeriod)
	{
		EXPECT_FALSE(tagMap.consumeTag(Cryptography::getSingleIdentificationTag(clientHash, 179, indexInPeriod)).has_value());
	}
	for (uint32_t periodIndex = 180; periodIndex <= 182; ++periodIndex)
	{
		for (uint32_t indexInPeriod = 0; indexInPeriod < PeriodSize; ++indexInPeriod)
		{
			EXPECT_TRUE(tagMap.consumeTag(Cryptography::getSingleIdentificationTag(clientHash, periodIndex, indexInPeriod)).has_value());
		}
	}
	for (uint32_t indexInPeriod = 0; indexInPeriod < PeriodSize; ++indexInPeriod)
	{
		EXPECT_FALSE(tagMap.consumeTag(Cryptography::getSingleIdentificationTag(clientHash, 183, indexInPeriod)).has_value());
	}
}

TEST(IdentificationTags, PopulatedTagMap_MovePeriodForwardByTwoAndConsumeAll_OnlyCorrectPeriodsAreConsumed)
{
	constexpr size_t PeriodSize = 16;
	Cryptography::IdentificationTagMap<PeriodSize> tagMap;

	std::vector<Cryptography::HashResult> clients;
	Cryptography::HashResult clientHash;
	fillWithTestRandomBytes(clientHash, getSessionRandomSeed());
	clients.push_back(clientHash.clone());
	tagMap.resetMap(clients, 200);

	tagMap.updateTagMap(clients, 200, 202);

	for (uint32_t periodIndex = 199; periodIndex <= 200; ++periodIndex)
	{
		for (uint32_t indexInPeriod = 0; indexInPeriod < PeriodSize; ++indexInPeriod)
		{
			EXPECT_FALSE(tagMap.consumeTag(Cryptography::getSingleIdentificationTag(clientHash, periodIndex, indexInPeriod)).has_value());
		}
	}
	for (uint32_t periodIndex = 201; periodIndex <= 203; ++periodIndex)
	{
		for (uint32_t indexInPeriod = 0; indexInPeriod < PeriodSize; ++indexInPeriod)
		{
			EXPECT_TRUE(tagMap.consumeTag(Cryptography::getSingleIdentificationTag(clientHash, periodIndex, indexInPeriod)).has_value());
		}
	}
	for (uint32_t indexInPeriod = 0; indexInPeriod < PeriodSize; ++indexInPeriod)
	{
		EXPECT_FALSE(tagMap.consumeTag(Cryptography::getSingleIdentificationTag(clientHash, 204, indexInPeriod)).has_value());
	}
}

TEST(IdentificationTags, PopulatedTagMap_MovePeriodForwardByThreeAndConsumeAll_OnlyCorrectPeriodsAreConsumed)
{
	constexpr size_t PeriodSize = 16;
	Cryptography::IdentificationTagMap<PeriodSize> tagMap;

	std::vector<Cryptography::HashResult> clients;
	Cryptography::HashResult clientHash;
	fillWithTestRandomBytes(clientHash, getSessionRandomSeed());
	clients.push_back(clientHash.clone());
	tagMap.resetMap(clients, 220);

	tagMap.updateTagMap(clients, 220, 223);

	for (uint32_t periodIndex = 219; periodIndex <= 221; ++periodIndex)
	{
		for (uint32_t indexInPeriod = 0; indexInPeriod < PeriodSize; ++indexInPeriod)
		{
			EXPECT_FALSE(tagMap.consumeTag(Cryptography::getSingleIdentificationTag(clientHash, periodIndex, indexInPeriod)).has_value());
		}
	}
	for (uint32_t periodIndex = 222; periodIndex <= 224; ++periodIndex)
	{
		for (uint32_t indexInPeriod = 0; indexInPeriod < PeriodSize; ++indexInPeriod)
		{
			EXPECT_TRUE(tagMap.consumeTag(Cryptography::getSingleIdentificationTag(clientHash, periodIndex, indexInPeriod)).has_value());
		}
	}
	for (uint32_t indexInPeriod = 0; indexInPeriod < PeriodSize; ++indexInPeriod)
	{
		EXPECT_FALSE(tagMap.consumeTag(Cryptography::getSingleIdentificationTag(clientHash, 225, indexInPeriod)).has_value());
	}
}

TEST(IdentificationTags, PopulatedTagMap_MovePeriodForwardByTenAndConsumeAll_OnlyCorrectPeriodsAreConsumed)
{
	constexpr size_t PeriodSize = 16;
	Cryptography::IdentificationTagMap<PeriodSize> tagMap;

	std::vector<Cryptography::HashResult> clients;
	Cryptography::HashResult clientHash;
	fillWithTestRandomBytes(clientHash, getSessionRandomSeed());
	clients.push_back(clientHash.clone());
	tagMap.resetMap(clients, 240);

	tagMap.updateTagMap(clients, 240, 250);

	for (uint32_t periodIndex = 239; periodIndex <= 248; ++periodIndex)
	{
		for (uint32_t indexInPeriod = 0; indexInPeriod < PeriodSize; ++indexInPeriod)
		{
			EXPECT_FALSE(tagMap.consumeTag(Cryptography::getSingleIdentificationTag(clientHash, periodIndex, indexInPeriod)).has_value());
		}
	}
	for (uint32_t periodIndex = 249; periodIndex <= 251; ++periodIndex)
	{
		for (uint32_t indexInPeriod = 0; indexInPeriod < PeriodSize; ++indexInPeriod)
		{
			EXPECT_TRUE(tagMap.consumeTag(Cryptography::getSingleIdentificationTag(clientHash, periodIndex, indexInPeriod)).has_value());
		}
	}
	for (uint32_t indexInPeriod = 0; indexInPeriod < PeriodSize; ++indexInPeriod)
	{
		EXPECT_FALSE(tagMap.consumeTag(Cryptography::getSingleIdentificationTag(clientHash, 252, indexInPeriod)).has_value());
	}
}

TEST(IdentificationTags, PopulatedTagMap_MovePeriodBackwardByOneAndConsumeAll_PeriodDoesNotChange)
{
	constexpr size_t PeriodSize = 16;
	Cryptography::IdentificationTagMap<PeriodSize> tagMap;

	std::vector<Cryptography::HashResult> clients;
	Cryptography::HashResult clientHash;
	fillWithTestRandomBytes(clientHash, getSessionRandomSeed());
	clients.push_back(clientHash.clone());
	tagMap.resetMap(clients, 260);

	tagMap.updateTagMap(clients, 260, 259);

	for (uint32_t indexInPeriod = 0; indexInPeriod < PeriodSize; ++indexInPeriod)
	{
		EXPECT_FALSE(tagMap.consumeTag(Cryptography::getSingleIdentificationTag(clientHash, 258, indexInPeriod)).has_value());
	}
	for (uint32_t periodIndex = 259; periodIndex <= 261; ++periodIndex)
	{
		for (uint32_t indexInPeriod = 0; indexInPeriod < PeriodSize; ++indexInPeriod)
		{
			EXPECT_TRUE(tagMap.consumeTag(Cryptography::getSingleIdentificationTag(clientHash, periodIndex, indexInPeriod)).has_value());
		}
	}
	for (uint32_t indexInPeriod = 0; indexInPeriod < PeriodSize; ++indexInPeriod)
	{
		EXPECT_FALSE(tagMap.consumeTag(Cryptography::getSingleIdentificationTag(clientHash, 262, indexInPeriod)).has_value());
	}
}

TEST(IdentificationTags, PopulatedTagMap_MovePeriodBackwardByTwoAndConsumeAll_PeriodDoesNotChange)
{
	constexpr size_t PeriodSize = 16;
	Cryptography::IdentificationTagMap<PeriodSize> tagMap;

	std::vector<Cryptography::HashResult> clients;
	Cryptography::HashResult clientHash;
	fillWithTestRandomBytes(clientHash, getSessionRandomSeed());
	clients.push_back(clientHash.clone());
	tagMap.resetMap(clients, 280);

	tagMap.updateTagMap(clients, 280, 278);

	for (uint32_t periodIndex = 277; periodIndex <= 278; ++periodIndex)
	{
		for (uint32_t indexInPeriod = 0; indexInPeriod < PeriodSize; ++indexInPeriod)
		{
			EXPECT_FALSE(tagMap.consumeTag(Cryptography::getSingleIdentificationTag(clientHash, periodIndex, indexInPeriod)).has_value());
		}
	}
	for (uint32_t periodIndex = 279; periodIndex <= 281; ++periodIndex)
	{
		for (uint32_t indexInPeriod = 0; indexInPeriod < PeriodSize; ++indexInPeriod)
		{
			EXPECT_TRUE(tagMap.consumeTag(Cryptography::getSingleIdentificationTag(clientHash, periodIndex, indexInPeriod)).has_value());
		}
	}
	for (uint32_t indexInPeriod = 0; indexInPeriod < PeriodSize; ++indexInPeriod)
	{
		EXPECT_FALSE(tagMap.consumeTag(Cryptography::getSingleIdentificationTag(clientHash, 282, indexInPeriod)).has_value());
	}
}

TEST(IdentificationTags, PopulatedTagMap_ResetPeriodBackwardByThreeAndConsumeAll_OnlyTheNewPeriodConsumed)
{
	constexpr size_t PeriodSize = 16;
	Cryptography::IdentificationTagMap<PeriodSize> tagMap;

	std::vector<Cryptography::HashResult> clients;
	Cryptography::HashResult clientHash;
	fillWithTestRandomBytes(clientHash, getSessionRandomSeed());
	clients.push_back(clientHash.clone());
	tagMap.resetMap(clients, 300);

	tagMap.resetMap(clients, 297);

	for (uint32_t indexInPeriod = 0; indexInPeriod < PeriodSize; ++indexInPeriod)
	{
		EXPECT_FALSE(tagMap.consumeTag(Cryptography::getSingleIdentificationTag(clientHash, 295, indexInPeriod)).has_value());
	}
	for (uint32_t periodIndex = 296; periodIndex <= 298; ++periodIndex)
	{
		for (uint32_t indexInPeriod = 0; indexInPeriod < PeriodSize; ++indexInPeriod)
		{
			EXPECT_TRUE(tagMap.consumeTag(Cryptography::getSingleIdentificationTag(clientHash, periodIndex, indexInPeriod)).has_value());
		}
	}
	for (uint32_t periodIndex = 299; periodIndex <= 301; ++periodIndex)
	{
		for (uint32_t indexInPeriod = 0; indexInPeriod < PeriodSize; ++indexInPeriod)
		{
			EXPECT_FALSE(tagMap.consumeTag(Cryptography::getSingleIdentificationTag(clientHash, periodIndex, indexInPeriod)).has_value());
		}
	}
}

TEST(IdentificationTags, PartiallyConsumedPopulatedTagMap_MovePeriodForwardByOneAndConsumeAll_AlreadyConsumedRecordsDoNotExist)
{
	constexpr size_t PeriodSize = 16;
	Cryptography::IdentificationTagMap<PeriodSize> tagMap;

	std::vector<Cryptography::HashResult> clients;
	Cryptography::HashResult clientHash;
	fillWithTestRandomBytes(clientHash, getSessionRandomSeed());
	clients.push_back(clientHash.clone());
	std::array<size_t, 3> consumedInPeriod = { 8, 11, 3 };
	tagMap.resetMap(clients, 320);

	for (size_t i = 0; i < 3; ++i)
	{
		for (uint32_t indexInPeriod = 0; indexInPeriod < consumedInPeriod[i]; ++indexInPeriod)
		{
			EXPECT_TRUE(tagMap.consumeTag(Cryptography::getSingleIdentificationTag(clientHash, 319 + i, indexInPeriod)).has_value());
		}
	}

	tagMap.updateTagMap(clients, 320, 321);

	for (uint32_t indexInPeriod = 0; indexInPeriod < PeriodSize; ++indexInPeriod)
	{
		EXPECT_FALSE(tagMap.consumeTag(Cryptography::getSingleIdentificationTag(clientHash, 319, indexInPeriod)).has_value());
	}
	// we skip i == 0 (period 319), since that one is gone and this fact is checked above
	for (size_t i = 1; i < 3; ++i)
	{
		for (uint32_t indexInPeriod = 0; indexInPeriod < PeriodSize; ++indexInPeriod)
		{
			EXPECT_EQ(indexInPeriod >= consumedInPeriod[i], tagMap.consumeTag(Cryptography::getSingleIdentificationTag(clientHash, 319 + i, indexInPeriod)).has_value());
		}
	}
	// this is the completely new period
	for (uint32_t indexInPeriod = 0; indexInPeriod < PeriodSize; ++indexInPeriod)
	{
		EXPECT_TRUE(tagMap.consumeTag(Cryptography::getSingleIdentificationTag(clientHash, 322, indexInPeriod)).has_value());
	}
	for (uint32_t indexInPeriod = 0; indexInPeriod < PeriodSize; ++indexInPeriod)
	{
		EXPECT_FALSE(tagMap.consumeTag(Cryptography::getSingleIdentificationTag(clientHash, 323, indexInPeriod)).has_value());
	}
}

TEST(IdentificationTags, TagMapPopulatedForTwoClients_ConsumeTags_TagsReturnCorrectClientIndex)
{
	constexpr size_t PeriodSize = 16;
	Cryptography::IdentificationTagMap<PeriodSize> tagMap;
	std::vector<Cryptography::HashResult> clients;
	Cryptography::HashResult clientHash1;
	fillWithTestRandomBytes(clientHash1, getSessionRandomSeed());
	clients.push_back(clientHash1.clone());
	Cryptography::HashResult clientHash2;
	fillWithTestRandomBytes(clientHash2, getSessionRandomSeed() + 1);
	clients.push_back(clientHash2.clone());
	tagMap.resetMap(clients, 340);

	for (uint32_t periodIndex = 339; periodIndex <= 341; ++periodIndex)
	{
		for (uint32_t indexInPeriod = 0; indexInPeriod < PeriodSize; ++indexInPeriod)
		{
			auto value1 = tagMap.consumeTag(Cryptography::getSingleIdentificationTag(clientHash1, periodIndex, indexInPeriod));
			ASSERT_TRUE(value1.has_value());
			EXPECT_EQ(value1->clientIndex, static_cast<uint32_t>(0));
			EXPECT_EQ(value1->periodIndex, periodIndex);
			EXPECT_EQ(value1->keyIndexInPeriod, indexInPeriod);

			auto value2 = tagMap.consumeTag(Cryptography::getSingleIdentificationTag(clientHash2, periodIndex, indexInPeriod));
			ASSERT_TRUE(value2.has_value());
			EXPECT_EQ(value2->clientIndex, static_cast<uint32_t>(1));
			EXPECT_EQ(value2->periodIndex, periodIndex);
			EXPECT_EQ(value2->keyIndexInPeriod, indexInPeriod);
		}
	}
}

TEST(IdentificationTags, TagMapPopulatedForTwoClients_MoveForwardByOneAndConsumeTags_OnlyCorrectPeriodsConsumed)
{
	constexpr size_t PeriodSize = 16;
	Cryptography::IdentificationTagMap<PeriodSize> tagMap;
	std::vector<Cryptography::HashResult> clients;
	Cryptography::HashResult clientHash1;
	fillWithTestRandomBytes(clientHash1, getSessionRandomSeed());
	clients.push_back(clientHash1.clone());
	Cryptography::HashResult clientHash2;
	fillWithTestRandomBytes(clientHash2, getSessionRandomSeed() + 1);
	clients.push_back(clientHash2.clone());
	tagMap.resetMap(clients, 360);

	tagMap.updateTagMap(clients, 360, 361);

	for (uint32_t indexInPeriod = 0; indexInPeriod < PeriodSize; ++indexInPeriod)
	{
		EXPECT_FALSE(tagMap.consumeTag(Cryptography::getSingleIdentificationTag(clientHash1, 359, indexInPeriod)).has_value());
		EXPECT_FALSE(tagMap.consumeTag(Cryptography::getSingleIdentificationTag(clientHash2, 359, indexInPeriod)).has_value());
	}
	for (uint32_t periodIndex = 360; periodIndex <= 362; ++periodIndex)
	{
		for (uint32_t indexInPeriod = 0; indexInPeriod < PeriodSize; ++indexInPeriod)
		{
			auto value1 = tagMap.consumeTag(Cryptography::getSingleIdentificationTag(clientHash1, periodIndex, indexInPeriod));
			ASSERT_TRUE(value1.has_value());
			EXPECT_EQ(value1->clientIndex, static_cast<uint32_t>(0));
			EXPECT_EQ(value1->periodIndex, periodIndex);
			EXPECT_EQ(value1->keyIndexInPeriod, indexInPeriod);

			auto value2 = tagMap.consumeTag(Cryptography::getSingleIdentificationTag(clientHash2, periodIndex, indexInPeriod));
			ASSERT_TRUE(value2.has_value());
			EXPECT_EQ(value2->clientIndex, static_cast<uint32_t>(1));
			EXPECT_EQ(value2->periodIndex, periodIndex);
			EXPECT_EQ(value2->keyIndexInPeriod, indexInPeriod);
		}
	}
	for (uint32_t indexInPeriod = 0; indexInPeriod < PeriodSize; ++indexInPeriod)
	{
		EXPECT_FALSE(tagMap.consumeTag(Cryptography::getSingleIdentificationTag(clientHash1, 363, indexInPeriod)).has_value());
		EXPECT_FALSE(tagMap.consumeTag(Cryptography::getSingleIdentificationTag(clientHash2, 363, indexInPeriod)).has_value());
	}
}

TEST(IdentificationTags, TagMapPopulatedForOneClients_AddOneClientTagsAndConsumeTags_BothClientTagsExist)
{
	constexpr size_t PeriodSize = 16;
	Cryptography::IdentificationTagMap<PeriodSize> tagMap;
	std::vector<Cryptography::HashResult> clients;
	Cryptography::HashResult clientHash1;
	fillWithTestRandomBytes(clientHash1, getSessionRandomSeed());
	clients.push_back(clientHash1.clone());
	tagMap.resetMap(clients, 380);

	Cryptography::HashResult clientHash2;
	fillWithTestRandomBytes(clientHash2, getSessionRandomSeed() + 1);
	clients.push_back(clientHash2.clone());
	tagMap.appendOneClientTagsForPeriods(clientHash2, 1, 379, 381);

	for (uint32_t periodIndex = 379; periodIndex <= 381; ++periodIndex)
	{
		for (uint32_t indexInPeriod = 0; indexInPeriod < PeriodSize; ++indexInPeriod)
		{
			auto value1 = tagMap.consumeTag(Cryptography::getSingleIdentificationTag(clientHash1, periodIndex, indexInPeriod));
			ASSERT_TRUE(value1.has_value());
			EXPECT_EQ(value1->clientIndex, static_cast<uint32_t>(0));
			EXPECT_EQ(value1->periodIndex, periodIndex);
			EXPECT_EQ(value1->keyIndexInPeriod, indexInPeriod);

			auto value2 = tagMap.consumeTag(Cryptography::getSingleIdentificationTag(clientHash2, periodIndex, indexInPeriod));
			ASSERT_TRUE(value2.has_value());
			EXPECT_EQ(value2->clientIndex, static_cast<uint32_t>(1));
			EXPECT_EQ(value2->periodIndex, periodIndex);
			EXPECT_EQ(value2->keyIndexInPeriod, indexInPeriod);
		}
	}
}

TEST(IdentificationTags, TagMapPopulatedForTwoClients_RemoveOneClientTagsAndConsumeTags_OnlyTagsFromRemainingClientExist)
{
	constexpr size_t PeriodSize = 16;
	Cryptography::IdentificationTagMap<PeriodSize> tagMap;
	std::vector<Cryptography::HashResult> clients;
	Cryptography::HashResult clientHash1;
	fillWithTestRandomBytes(clientHash1, getSessionRandomSeed());
	clients.push_back(clientHash1.clone());
	Cryptography::HashResult clientHash2;
	fillWithTestRandomBytes(clientHash2, getSessionRandomSeed() + 1);
	clients.push_back(clientHash2.clone());
	tagMap.resetMap(clients, 400);

	tagMap.removeClientTags(0);

	for (uint32_t periodIndex = 399; periodIndex <= 401; ++periodIndex)
	{
		for (uint32_t indexInPeriod = 0; indexInPeriod < PeriodSize; ++indexInPeriod)
		{
			auto value1 = tagMap.consumeTag(Cryptography::getSingleIdentificationTag(clientHash1, periodIndex, indexInPeriod));
			EXPECT_FALSE(value1.has_value());

			auto value2 = tagMap.consumeTag(Cryptography::getSingleIdentificationTag(clientHash2, periodIndex, indexInPeriod));
			ASSERT_TRUE(value2.has_value());
			EXPECT_EQ(value2->clientIndex, static_cast<uint32_t>(1));
			EXPECT_EQ(value2->periodIndex, periodIndex);
			EXPECT_EQ(value2->keyIndexInPeriod, indexInPeriod);
		}
	}
}
