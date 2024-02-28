#include "Misc.h"
#include <cstring>
#include <algorithm>

namespace XSP
{

	static int32_t const gsLeadingBitTable[32] =
	{
		 0,  9,  1, 10, 13, 21,  2, 29,
		11, 14, 16, 18, 22, 25,  3, 30,
		 8, 12, 20, 28, 15, 17, 24,  7,
		19, 27, 23,  6, 26,  5,  4, 31
	};

	int32_t GetLeadingBit(uint32_t value)
	{
		value |= value >> 1;
		value |= value >> 2;
		value |= value >> 4;
		value |= value >> 8;
		value |= value >> 16;
		uint32_t key = (value * 0x07C4ACDDu) >> 27;
		return gsLeadingBitTable[key];
	}

	uint64_t RoundUpToPowerOfTwo(uint32_t value)
	{
		if (value > 0)
		{
			int32_t leading = GetLeadingBit(value);
			uint32_t mask = (1 << leading);
			if ((value & ~mask) == 0)
			{
				// value is a power of two
				return static_cast<uint64_t>(value);
			}
			else
			{
				// round up to a power of two
				return (static_cast<uint64_t>(mask) << 1);
			}

		}
		else
		{
			return 1ull;
		}
	}

	float ComputeSquaredDistanceFromBoxToPoint(const FVector3f& Mins, const FVector3f& Maxs, const FVector3f& Point)
	{
		// Accumulates the distance as we iterate axis
		float DistSquared = 0;

		// Check each axis for min/max and add the distance accordingly
		// NOTE: Loop manually unrolled for > 2x speed up
		if (Point.X < Mins.X)
		{
			DistSquared += Square(Point.X - Mins.X);
		}
		else if (Point.X > Maxs.X)
		{
			DistSquared += Square(Point.X - Maxs.X);
		}

		if (Point.Y < Mins.Y)
		{
			DistSquared += Square(Point.Y - Mins.Y);
		}
		else if (Point.Y > Maxs.Y)
		{
			DistSquared += Square(Point.Y - Maxs.Y);
		}

		if (Point.Z < Mins.Z)
		{
			DistSquared += Square(Point.Z - Mins.Z);
		}
		else if (Point.Z > Maxs.Z)
		{
			DistSquared += Square(Point.Z - Maxs.Z);
		}

		return DistSquared;
	}


	FHashTable::FHashTable(uint32_t InHashSize, uint32_t InIndexSize)
		: HashSize(InHashSize)
		, HashMask(0)
		, IndexSize(InIndexSize)
		, Hash(EmptyHash)
		, NextIndex(nullptr)
	{
		if (IndexSize)
		{
			HashMask = HashSize - 1;

			Hash = new uint32_t[HashSize];
			NextIndex = new uint32_t[IndexSize];

			std::memset(Hash, 0xff, HashSize * 4);
		}
	}

	FHashTable::FHashTable(const FHashTable& Other)
		: HashSize(Other.HashSize)
		, HashMask(Other.HashMask)
		, IndexSize(Other.IndexSize)
		, Hash(EmptyHash)
	{
		if (IndexSize)
		{
			Hash = new uint32_t[HashSize];
			NextIndex = new uint32_t[IndexSize];

			std::memcpy(Hash, Other.Hash, HashSize * 4);
			std::memcpy(NextIndex, Other.NextIndex, IndexSize * 4);
		}
	}

	FHashTable::~FHashTable()
	{
		Free();
	}

	void FHashTable::Clear()
	{
		if (IndexSize)
		{
			std::memset(Hash, 0xff, HashSize * 4);
		}
	}

	void FHashTable::Clear(uint32_t InHashSize, uint32_t InIndexSize)
	{
		Free();

		HashSize = InHashSize;
		IndexSize = InIndexSize;

		if (IndexSize)
		{
			HashMask = HashSize - 1;

			Hash = new uint32_t[HashSize];
			NextIndex = new uint32_t[IndexSize];

			std::memset(Hash, 0xff, HashSize * 4);
		}
	}

	void FHashTable::Free()
	{
		if (IndexSize)
		{
			HashMask = 0;
			IndexSize = 0;

			delete[] Hash;
			Hash = EmptyHash;

			delete[] NextIndex;
			NextIndex = nullptr;
		}
	}

	// First in hash chain
	uint32_t FHashTable::First(uint32_t Key) const
	{
		Key &= HashMask;
		return Hash[Key];
	}

	// Next in hash chain
	uint32_t FHashTable::Next(uint32_t Index) const
	{
		return NextIndex[Index];
	}

	bool FHashTable::IsValid(uint32_t Index) const
	{
		return Index != ~0u;
	}

	void FHashTable::Add(uint32_t Key, uint32_t Index)
	{
		if (Index >= IndexSize)
		{
			Resize(std::max((uint64_t)32u, RoundUpToPowerOfTwo(Index + 1)));
		}

		Key &= HashMask;
		NextIndex[Index] = Hash[Key];
		Hash[Key] = Index;
	}

	// Safe for many threads to add concurrently.
	// Not safe to search the table while other threads are adding.
	// Will not resize. Only use for presized tables.
	//void FHashTable::Add_Concurrent(uint32_t Key, uint32_t Index)
	//{
	//	Key &= HashMask;
	//	NextIndex[Index] = FPlatformAtomics::InterlockedExchange((int32_t*)&Hash[Key], Index);
	//}

	uint32_t FHashTable::EmptyHash[1] = { ~0u };

	void FHashTable::Resize(uint32_t NewIndexSize)
	{
		if (NewIndexSize == IndexSize)
		{
			return;
		}

		if (NewIndexSize == 0)
		{
			Free();
			return;
		}

		if (IndexSize == 0)
		{
			HashMask = (uint16_t)(HashSize - 1);
			Hash = new uint32_t[HashSize];
			std::memset(Hash, 0xff, HashSize * 4);
		}

		uint32_t* NewNextIndex = new uint32_t[NewIndexSize];

		if (NextIndex)
		{
			std::memcpy(NewNextIndex, NextIndex, IndexSize * 4);
			delete[] NextIndex;
		}

		IndexSize = NewIndexSize;
		NextIndex = NewNextIndex;
	}

	float FHashTable::AverageSearch() const
	{
		uint32_t SumAvgSearch = 0;
		uint32_t NumElements = 0;
		for (uint32_t Key = 0; Key < HashSize; Key++)
		{
			uint32_t NumInBucket = 0;
			for (uint32_t i = First((uint16_t)Key); IsValid(i); i = Next(i))
			{
				NumInBucket++;
			}

			SumAvgSearch += NumInBucket * (NumInBucket + 1);
			NumElements += NumInBucket;
		}
		return (float)(SumAvgSearch >> 1) / (float)NumElements;
	}

}