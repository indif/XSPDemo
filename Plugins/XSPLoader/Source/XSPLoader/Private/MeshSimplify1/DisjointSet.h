// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

//#include "CoreMinimal.h"
#include <vector>

namespace XSP
{

	class FDisjointSet
	{
	public:
		FDisjointSet() {}
		FDisjointSet(const uint32_t Size);

		void	Init(uint32_t Size);
		void	Reset();
		void	AddDefaulted(uint32_t Num = 1);

		void	Union(uint32_t x, uint32_t y);
		void	UnionSequential(uint32_t x, uint32_t y);
		uint32_t	Find(uint32_t i);

		uint32_t	operator[](uint32_t i) const { return Parents[i]; }

	private:
		std::vector< uint32_t >	Parents;
	};

	// Union with splicing
	inline void FDisjointSet::Union(uint32_t x, uint32_t y)
	{
		uint32_t px = Parents[x];
		uint32_t py = Parents[y];

		while (px != py)
		{
			// Pick larger
			if (px < py)
			{
				Parents[x] = py;
				if (x == px)
				{
					return;
				}
				x = px;
				px = Parents[x];
			}
			else
			{
				Parents[y] = px;
				if (y == py)
				{
					return;
				}
				y = py;
				py = Parents[y];
			}
		}
	}

	// Optimized version of Union when iterating for( x : 0 to N ) unioning x with lower indexes.
	// Neither x nor y can have already been unioned with an index > x.
	inline void FDisjointSet::UnionSequential(uint32_t x, uint32_t y)
	{
		uint32_t px = x;
		uint32_t py = Parents[y];
		while (px != py)
		{
			Parents[y] = px;
			if (y == py)
			{
				return;
			}
			y = py;
			py = Parents[y];
		}
	}

	// Find with path compression
	inline uint32_t FDisjointSet::Find(uint32_t i)
	{
		// Find root
		uint32_t Start = i;
		uint32_t Root = Parents[i];
		while (Root != i)
		{
			i = Root;
			Root = Parents[i];
		}

		// Point all nodes on path to root
		i = Start;
		uint32_t Parent = Parents[i];
		while (Parent != Root)
		{
			Parents[i] = Root;
			i = Parent;
			Parent = Parents[i];
		}

		return Root;
	}
}