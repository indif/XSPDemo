#pragma once
#include "Misc.h"
#include "BinaryHeap.h"
#include "Quadric.h"
#include "DisjointSet.h"
#include <vector>

namespace XspMeshSimp
{

	class FMeshSimplifier
	{
	public:
		FMeshSimplifier(float* Verts, uint32_t NumVerts, uint32_t* Indexes, uint32_t NumIndexes, int32_t* MaterialIndexes, uint32_t NumAttributes);
		~FMeshSimplifier() = default;

		void		SetAttributeWeights(const float* Weights) { AttributeWeights = Weights; }
		void		SetEdgeWeight(float Weight) { EdgeWeight = Weight; }
		void		SetCorrectAttributes(void (*Function)(float*)) { CorrectAttributes = Function; }
		void		SetLimitErrorToSurfaceArea(bool Value) { bLimitErrorToSurfaceArea = Value; }

		void	LockPosition(const FVector3f& Position);

		float	Simplify(
			uint32_t TargetNumVerts, uint32_t TargetNumTris, float TargetError,
			uint32_t LimitNumVerts, uint32_t LimitNumTris, float LimitError);
		//void	PreserveSurfaceArea();
		//void	DumpOBJ(const char* Filename);
		void	Compact();

		uint32_t		GetRemainingNumVerts() const { return RemainingNumVerts; }
		uint32_t		GetRemainingNumTris() const { return RemainingNumTris; }

		int32_t DegreeLimit = 24;
		float DegreePenalty = 0.5f;
		float LockPenalty = 1e8f;
		float InversionPenalty = 100.0f;

	protected:
		uint32_t		NumVerts;
		uint32_t		NumIndexes;
		uint32_t		NumAttributes;
		uint32_t		NumTris;

		uint32_t		RemainingNumVerts;
		uint32_t		RemainingNumTris;

		float* Verts;
		uint32_t* Indexes;
		int32_t* MaterialIndexes;

		const float* AttributeWeights = nullptr;
		float			EdgeWeight = 8.0f;
		void			(*CorrectAttributes)(float*) = nullptr;
		bool			bLimitErrorToSurfaceArea = true;

		FHashTable		VertHash;
		FHashTable		CornerHash;

		std::vector<uint32_t>	VertRefCount;
		std::vector<uint8_t>		CornerFlags;
		std::vector<bool>			TriRemoved;

		struct FPerMaterialDeltas
		{
			float	SurfaceArea;
			int32_t	NumTris;
			int32_t	NumDisjoint;
		};
		std::vector< FPerMaterialDeltas >	PerMaterialDeltas;

		struct FPair
		{
			FVector3f	Position0;
			FVector3f	Position1;
		};
		std::vector< FPair >			Pairs;
		FHashTable				PairHash0;
		FHashTable				PairHash1;
		FBinaryHeap< float >	PairHeap;

		std::vector< uint32_t >	MovedVerts;
		std::vector< uint32_t >	MovedCorners;
		std::vector< uint32_t >	MovedPairs;
		std::vector< uint32_t >	ReevaluatePairs;

		std::vector< uint8_t >		TriQuadrics;
		std::vector< FEdgeQuadric >	EdgeQuadrics;
		std::vector<bool>			EdgeQuadricsValid;

		std::vector< float > WedgeAttributes;
		FDisjointSet	WedgeDisjointSet;

		enum ECornerFlags
		{
			MergeMask = 3,		// Merge position 0 or 1
			AdjTriMask = (1 << 2),	// Has been added to AdjTris
			LockedVertMask = (1 << 3),	// Vert is locked, disallowing position movement
			RemoveTriMask = (1 << 4),	// Triangle will overlap another after merge and should be removed
		};

	protected:
		FVector3f& GetPosition(uint32_t VertIndex);
		const FVector3f& GetPosition(uint32_t VertIndex) const;
		float* GetAttributes(uint32_t VertIndex);
		FQuadricAttr& GetTriQuadric(uint32_t TriIndex);

		template< typename FuncType >
		void	ForAllVerts(const FVector3f& Position, FuncType&& Function) const;

		template< typename FuncType >
		void	ForAllCorners(const FVector3f& Position, FuncType&& Function) const;

		template< typename FuncType >
		void	ForAllPairs(const FVector3f& Position, FuncType&& Function) const;

		void	GatherAdjTris(const FVector3f& Position, uint32_t Flag, std::vector< uint32_t>& AdjTris, int32_t& VertDegree, uint32_t& FlagUnion);
		bool	AddUniquePair(FPair& Pair, uint32_t PairIndex);

		void	CalcTriQuadric(uint32_t TriIndex);
		void	CalcEdgeQuadric(uint32_t EdgeIndex);

		float	EvaluateMerge(const FVector3f& Position0, const FVector3f& Position1, bool bMoveVerts);

		void	BeginMovePosition(const FVector3f& Position);
		void	EndMovePositions();

		uint32_t	CornerIndexMoved(uint32_t TriIndex) const;
		bool	TriWillInvert(uint32_t TriIndex, const FVector3f& NewPosition) const;

		void	FixUpTri(uint32_t TriIndex);
		bool	IsDuplicateTri(uint32_t TriIndex) const;
		void	SetVertIndex(uint32_t Corner, uint32_t NewVertIndex);
		void	RemoveDuplicateVerts(uint32_t Corner);
	};


	inline FVector3f& FMeshSimplifier::GetPosition(uint32_t VertIndex)
	{
		return *reinterpret_cast<FVector3f*>(&Verts[(3 + NumAttributes) * VertIndex]);
	}

	inline const FVector3f& FMeshSimplifier::GetPosition(uint32_t VertIndex) const
	{
		return *reinterpret_cast<const FVector3f*>(&Verts[(3 + NumAttributes) * VertIndex]);
	}

	inline float* FMeshSimplifier::GetAttributes(uint32_t VertIndex)
	{
		return &Verts[(3 + NumAttributes) * VertIndex + 3];
	}

	inline FQuadricAttr& FMeshSimplifier::GetTriQuadric(uint32_t TriIndex)
	{
		const unsigned __int64 QuadricSize = sizeof(FQuadricAttr) + NumAttributes * 4 * sizeof(QScalar);
		return *reinterpret_cast<FQuadricAttr*>(&TriQuadrics[TriIndex * QuadricSize]);
	}

	template< typename FuncType >
	void FMeshSimplifier::ForAllVerts(const FVector3f& Position, FuncType&& Function) const
	{
		uint32_t Hash = HashPosition(Position);
		for (uint32_t VertIndex = VertHash.First(Hash); VertHash.IsValid(VertIndex); VertIndex = VertHash.Next(VertIndex))
		{
			if (GetPosition(VertIndex) == Position)
			{
				Function(VertIndex);
			}
		}
	}

	template< typename FuncType >
	void FMeshSimplifier::ForAllCorners(const FVector3f& Position, FuncType&& Function) const
	{
		uint32_t Hash = HashPosition(Position);
		for (uint32_t Corner = CornerHash.First(Hash); CornerHash.IsValid(Corner); Corner = CornerHash.Next(Corner))
		{
			if (GetPosition(Indexes[Corner]) == Position)
			{
				Function(Corner);
			}
		}
	}

	template< typename FuncType >
	void FMeshSimplifier::ForAllPairs(const FVector3f& Position, FuncType&& Function) const
	{
		uint32_t Hash = HashPosition(Position);
		for (uint32_t PairIndex = PairHash0.First(Hash); PairHash0.IsValid(PairIndex); PairIndex = PairHash0.Next(PairIndex))
		{
			if (Pairs[PairIndex].Position0 == Position)
			{
				Function(PairIndex);
			}
		}

		for (uint32_t PairIndex = PairHash1.First(Hash); PairHash1.IsValid(PairIndex); PairIndex = PairHash1.Next(PairIndex))
		{
			if (Pairs[PairIndex].Position1 == Position)
			{
				Function(PairIndex);
			}
		}
	}

	bool SimplyMesh(const std::vector<FVector3f>& InPositions, float PercentTriangles, float PercentVertices, std::vector<FVector3f>& OutPositions, std::vector<FVector3f>& OutNormals, std::vector<uint32_t>& OutIndices);
}