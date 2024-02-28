#include "DisjointSet.h"

namespace XspMeshSimp
{

	FDisjointSet::FDisjointSet(const uint32_t Size)
	{
		Init(Size);
	}

	void FDisjointSet::Init(uint32_t Size)
	{
		Parents.resize(Size);
		//	Parents.SetNumUninitialized( Size, false );
		for (uint32_t i = 0; i < Size; i++)
		{
			Parents[i] = i;
		}
	}

	void FDisjointSet::Reset()
	{
		Parents.clear();
		//Parents.Reset();
	}

	void FDisjointSet::AddDefaulted(uint32_t Num)
	{
		uint32_t Start = Parents.size();
		Parents.insert(Parents.end(), Num, 0);
		//uint32_t Start = Parents.AddUninitialized( Num );

		for (uint32_t i = Start; i < Start + Num; i++)
		{
			Parents[i] = i;
		}
	}

}