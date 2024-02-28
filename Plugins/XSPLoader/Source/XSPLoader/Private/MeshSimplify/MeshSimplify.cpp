#include "MeshSimplify.h"
#include "Misc.h"

#include <set>
#include <map>
#include <algorithm>
#include <cmath>

namespace XspMeshSimp
{


struct FIndexAndZ
{
	float Z;
	int32_t Index;

	/** Default constructor. */
	FIndexAndZ() {}

	/** Initialization constructor. */
	FIndexAndZ(int32_t InIndex, FVector3f V)
	{
		Z = 0.30f * V.X + 0.33f * V.Y + 0.37f * V.Z;
		Index = InIndex;
	}

	void Init(int32_t InIndex, FVector3f V)
	{
		Z = 0.30f * V.X + 0.33f * V.Y + 0.37f * V.Z;
		Index = InIndex;
	}
};

/** Sorting function for vertex Z/index pairs. */
struct FCompareIndexAndZ
{
	bool operator()(FIndexAndZ const& A, FIndexAndZ const& B) const { return A.Z < B.Z; }
};

inline bool PointsEqual(const FVector3f& V1, const FVector3f& V2)
{
	const float Epsilon = XSM_THRESH_POINTS_ARE_SAME;
	return std::abs(V1.X - V2.X) <= Epsilon && std::abs(V1.Y - V2.Y) <= Epsilon && std::abs(V1.Z - V2.Z) <= Epsilon;
}

inline bool NormalsEqual(const FVector3f& V1, const FVector3f& V2)
{
	const float Epsilon = XSM_THRESH_NORMALS_ARE_SAME;
	return std::abs(V1.X - V2.X) <= Epsilon && std::abs(V1.Y - V2.Y) <= Epsilon && std::abs(V1.Z - V2.Z) <= Epsilon;
}

struct FOverlappingCorners
{
	FOverlappingCorners() {}

	FOverlappingCorners(const std::vector<FVector3f>& InVertices, const std::vector<uint32_t>& InIndices);

	/* Resets, pre-allocates memory, marks all indices as not overlapping in preperation for calls to Add() */
	void Init(int32_t NumIndices);

	/* Add overlapping indices pair */
	void Add(int32_t Key, int32_t Value);

	/* Sorts arrays, converts sets to arrays for sorting and to allow simple iterating code, prevents additional adding */
	void FinishAdding();

	/* Estimate memory allocated */
	//uint32_t GetAllocatedSize(void) const;

	/**
	* @return array of sorted overlapping indices including input 'Key', empty array for indices that have no overlaps.
	*/
	const std::vector<int32_t>& FindIfOverlapping(int32_t Key) const
	{
		int32_t ContainerIndex = IndexBelongsTo[Key];
		return (ContainerIndex != -1) ? Arrays[ContainerIndex] : EmptyArray;
	}

private:
	std::vector<int32_t> IndexBelongsTo;
	std::vector<std::vector<int32_t>> Arrays;
	std::vector<std::set<int32_t>> Sets;
	std::vector<int32_t> EmptyArray;
	bool bFinishedAdding = false;
};

FOverlappingCorners::FOverlappingCorners(const std::vector<FVector3f>& InVertices, const std::vector<uint32_t>& InIndices)
{
	const int32_t NumWedges = (int32_t)InIndices.size();

	// Create a list of vertex Z/index pairs
	std::vector<FIndexAndZ> VertIndexAndZ;
	VertIndexAndZ.resize(NumWedges);
	for (int32_t WedgeIndex = 0; WedgeIndex < NumWedges; WedgeIndex++)
	{
		VertIndexAndZ[WedgeIndex].Init(WedgeIndex, InVertices[InIndices[WedgeIndex]]);
	}

	// Sort the vertices by z value
	std::sort(VertIndexAndZ.begin(), VertIndexAndZ.end(), FCompareIndexAndZ());

	Init(NumWedges);

	// Search for duplicates, quickly!
	for (int32_t i = 0; i < VertIndexAndZ.size(); i++)
	{
		// only need to search forward, since we add pairs both ways
		for (int32_t j = i + 1; j < VertIndexAndZ.size(); j++)
		{
			if (std::abs(VertIndexAndZ[j].Z - VertIndexAndZ[i].Z) > XSM_THRESH_POINTS_ARE_SAME)
				break; // can't be any more dups

			const FVector3f& PositionA = InVertices[InIndices[VertIndexAndZ[i].Index]];
			const FVector3f& PositionB = InVertices[InIndices[VertIndexAndZ[j].Index]];

			if (PointsEqual(PositionA, PositionB))
			{
				Add(VertIndexAndZ[i].Index, VertIndexAndZ[j].Index);
			}
		}
	}

	FinishAdding();
}

void FOverlappingCorners::Init(int32_t NumIndices)
{
	Arrays.clear();
	Sets.clear();
	bFinishedAdding = false;

	IndexBelongsTo.assign(NumIndices, -1);
}

void FOverlappingCorners::Add(int32_t Key, int32_t Value)
{
	int32_t ContainerIndex = IndexBelongsTo[Key];
	if (ContainerIndex == -1)
	{
		ContainerIndex = (int32_t)Arrays.size();
		Arrays.push_back(std::vector<int32_t>());
		std::vector<int32_t>& Container = Arrays.back();
		Container.reserve(6);
		Container.push_back(Key);
		Container.push_back(Value);
		IndexBelongsTo[Key] = ContainerIndex;
		IndexBelongsTo[Value] = ContainerIndex;
	}
	else
	{
		IndexBelongsTo[Value] = ContainerIndex;

		std::vector<int32_t>& ArrayContainer = Arrays[ContainerIndex];
		if (ArrayContainer.size() == 1)
		{
			// Container is a set
			Sets[ArrayContainer.back()].insert(Value);
		}
		else
		{
			// Container is an array
			if (std::find(ArrayContainer.begin(), ArrayContainer.end(), Value) == ArrayContainer.end())
				ArrayContainer.push_back(Value);

			// Change container into set when one vertex is shared by large number of triangles
			if (ArrayContainer.size() > 12)
			{
				int32_t SetIndex = (int32_t)Sets.size();
				Sets.push_back(std::set<int32_t>());
				std::set<int32_t>& Set = Sets.back();
				Set.insert(ArrayContainer.begin(), ArrayContainer.end());

				// Having one element means we are using a set
				// An array will never have just 1 element normally because we add them as pairs
				ArrayContainer.clear();
				ArrayContainer.push_back(SetIndex);
			}
		}
	}
}

void FOverlappingCorners::FinishAdding()
{
	for (std::vector<int32_t>& Array : Arrays)
	{
		// Turn sets back into arrays for easier iteration code
		// Also reduces peak memory later in the import process
		if (Array.size() == 1)
		{
			std::set<int32_t>& Set = Sets[Array.back()];
			Array.clear();
			Array.insert(Array.begin(), Set.begin(), Set.end());
		}

		// Sort arrays now to avoid sort multiple times
		std::sort(Array.begin(), Array.end());
	}

	Sets.clear();

	bFinishedAdding = true;
}

struct FVertSimp
{
	FVector3f			Position;
	FVector3f			Normal;
	bool Equals(const FVertSimp& a) const
	{
		if (!PointsEqual(Position, a.Position) ||
			!NormalsEqual(Normal, a.Normal))
		{
			return false;
		}
		return true;
	}
};

static uint32_t MurmurFinalize32(uint32_t Hash)
{
	Hash ^= Hash >> 16;
	Hash *= 0x85ebca6b;
	Hash ^= Hash >> 13;
	Hash *= 0xc2b2ae35;
	Hash ^= Hash >> 16;
	return Hash;
}

static uint64_t MurmurFinalize64(uint64_t Hash)
{
	Hash ^= Hash >> 33;
	Hash *= 0xff51afd7ed558ccdull;
	Hash ^= Hash >> 33;
	Hash *= 0xc4ceb9fe1a85ec53ull;
	Hash ^= Hash >> 33;
	return Hash;
}

static uint32_t Murmur32(std::initializer_list< uint32_t > InitList)
{
	uint32_t Hash = 0;
	for (auto Element : InitList)
	{
		Element *= 0xcc9e2d51;
		Element = (Element << 15) | (Element >> (32 - 15));
		Element *= 0x1b873593;

		Hash ^= Element;
		Hash = (Hash << 13) | (Hash >> (32 - 13));
		Hash = Hash * 5 + 0xe6546b64;
	}

	return MurmurFinalize32(Hash);
}


uint32_t HashPosition(const FVector3f& Position)
{
	union { float f; uint32_t i; } x;
	union { float f; uint32_t i; } y;
	union { float f; uint32_t i; } z;

	x.f = Position.X;
	y.f = Position.Y;
	z.f = Position.Z;

	return Murmur32({
		Position.X == 0.0f ? 0u : x.i,
		Position.Y == 0.0f ? 0u : y.i,
		Position.Z == 0.0f ? 0u : z.i
		});
}

uint32_t Cycle3(uint32_t Value)
{
	uint32_t ValueMod3 = Value % 3;
	uint32_t Value1Mod3 = (1 << ValueMod3) & 3;
	return Value - ValueMod3 + Value1Mod3;
}

uint32_t Cycle3(uint32_t Value, uint32_t Offset)
{
	return Value - Value % 3 + (Value + Offset) % 3;
}

FMeshSimplifier1::FMeshSimplifier1(float* InVerts, uint32_t InNumVerts, uint32_t* InIndexes, uint32_t InNumIndexes, int32_t* InMaterialIndexes, uint32_t InNumAttributes)
	: NumVerts(InNumVerts)
	, NumIndexes(InNumIndexes)
	, NumAttributes(InNumAttributes)
	, NumTris(NumIndexes / 3)
	, RemainingNumVerts(NumVerts)
	, RemainingNumTris(NumTris)
	, Verts(InVerts)
	, Indexes(InIndexes)
	, MaterialIndexes(InMaterialIndexes)
	, VertHash(1 << std::min(16u, (uint32_t)std::floor(std::log2(NumVerts))))
	, CornerHash(1 << std::min(16u, (uint32_t)std::floor(std::log2(NumIndexes))))
	//, TriRemoved(false, NumTris)
{
	TriRemoved.insert(TriRemoved.end(), NumTris, false);
	for (uint32_t VertIndex = 0; VertIndex < NumVerts; VertIndex++)
	{
		VertHash.Add(HashPosition(GetPosition(VertIndex)), VertIndex);
	}

	VertRefCount.insert(VertRefCount.begin(), NumVerts, 0);//VertRefCount.AddZeroed(NumVerts);
	CornerFlags.insert(CornerFlags.begin(), NumIndexes, 0);//CornerFlags.AddZeroed(NumIndexes);

	EdgeQuadrics.resize(NumIndexes);//EdgeQuadrics.AddUninitialized(NumIndexes);

	EdgeQuadricsValid.insert(EdgeQuadricsValid.begin(), NumIndexes, false);//EdgeQuadricsValid.Init(false, NumIndexes);

	// Guess number of edges based on Euler's formula.
	uint32_t NumEdges = std::min(NumIndexes, std::min(3 * NumVerts - 6, NumTris + NumVerts));
	Pairs.reserve(NumEdges);//Pairs.Reserve(NumEdges);
	PairHash0.Clear(1 << std::min(16u, (uint32_t)std::floor(std::log2(NumEdges))), NumEdges);
	PairHash1.Clear(1 << std::min(16u, (uint32_t)std::floor(std::log2(NumEdges))), NumEdges);

	for (uint32_t Corner = 0; Corner < NumIndexes; Corner++)
	{
		uint32_t VertIndex = Indexes[Corner];

		VertRefCount[VertIndex]++;

		const FVector3f& Position = GetPosition(VertIndex);
		CornerHash.Add(HashPosition(Position), Corner);

		uint32_t OtherCorner = Cycle3(Corner);

		FPair Pair;
		Pair.Position0 = Position;
		Pair.Position1 = GetPosition(Indexes[Cycle3(Corner)]);

		if (AddUniquePair(Pair, Pairs.size()))
		{
			Pairs.push_back(Pair);//Pairs.Add(Pair);
		}
	}
}

void FMeshSimplifier1::LockPosition(const FVector3f& Position)
{
	ForAllCorners(Position,
		[this](uint32_t Corner)
		{
			CornerFlags[Corner] |= LockedVertMask;
		});
}

bool FMeshSimplifier1::AddUniquePair(FPair& Pair, uint32_t PairIndex)
{
	uint32_t Hash0 = HashPosition(Pair.Position0);
	uint32_t Hash1 = HashPosition(Pair.Position1);

	if (Hash0 > Hash1)
	{
		std::swap(Hash0, Hash1);//Swap(Hash0, Hash1);
		std::swap(Pair.Position0, Pair.Position1);//Swap(Pair.Position0, Pair.Position1);
	}

	uint32_t OtherPairIndex;
	for (OtherPairIndex = PairHash0.First(Hash0); PairHash0.IsValid(OtherPairIndex); OtherPairIndex = PairHash0.Next(OtherPairIndex))
	{
		FPair& OtherPair = Pairs[OtherPairIndex];

		if (Pair.Position0 == OtherPair.Position0 &&
			Pair.Position1 == OtherPair.Position1)
		{
			// Found a duplicate
			return false;
		}
	}

	PairHash0.Add(Hash0, PairIndex);
	PairHash1.Add(Hash1, PairIndex);

	return true;
}

void FMeshSimplifier1::CalcTriQuadric(uint32_t TriIndex)
{
	uint32_t i0 = Indexes[TriIndex * 3 + 0];
	uint32_t i1 = Indexes[TriIndex * 3 + 1];
	uint32_t i2 = Indexes[TriIndex * 3 + 2];

	new(&GetTriQuadric(TriIndex)) FQuadricAttr(
		GetPosition(i0),
		GetPosition(i1),
		GetPosition(i2),
		GetAttributes(i0),
		GetAttributes(i1),
		GetAttributes(i2),
		AttributeWeights,
		NumAttributes);
}

void FMeshSimplifier1::CalcEdgeQuadric(uint32_t EdgeIndex)
{
	uint32_t TriIndex = EdgeIndex / 3;
	if (TriRemoved[TriIndex])
	{
		EdgeQuadricsValid[EdgeIndex] = false;
		return;
	}

	int32_t MaterialIndex = MaterialIndexes[TriIndex];

	uint32_t VertIndex0 = Indexes[EdgeIndex];
	uint32_t VertIndex1 = Indexes[Cycle3(EdgeIndex)];

	const FVector3f& Position0 = GetPosition(VertIndex0);
	const FVector3f& Position1 = GetPosition(VertIndex1);

	// Find edge with opposite direction that shares these 2 verts.
	// If none then we need to add an edge constraint.
	/*
		  /\
		 /  \
		o-<<-o
		o->>-o
		 \  /
		  \/
	*/
	uint32_t Hash = HashPosition(Position1);
	uint32_t Corner;
	for (Corner = CornerHash.First(Hash); CornerHash.IsValid(Corner); Corner = CornerHash.Next(Corner))
	{
		uint32_t OtherVertIndex0 = Indexes[Corner];
		uint32_t OtherVertIndex1 = Indexes[Cycle3(Corner)];

		if (VertIndex0 == OtherVertIndex1 &&
			VertIndex1 == OtherVertIndex0 &&
			MaterialIndex == MaterialIndexes[Corner / 3])
		{
			// Found matching edge.
			// No constraints needed so remove any that exist.
			EdgeQuadricsValid[EdgeIndex] = false;
			return;
		}
	}

	// Don't double count attribute discontinuities.
	float Weight = EdgeWeight;
	for (Corner = CornerHash.First(Hash); CornerHash.IsValid(Corner); Corner = CornerHash.Next(Corner))
	{
		uint32_t OtherVertIndex0 = Indexes[Corner];
		uint32_t OtherVertIndex1 = Indexes[Cycle3(Corner)];

		if (Position0 == GetPosition(OtherVertIndex1) &&
			Position1 == GetPosition(OtherVertIndex0))
		{
			// Found matching edge.
			Weight *= 0.5f;
			break;
		}
	}

	const QVec3 p0 = GetPosition(Indexes[TriIndex * 3 + 0]);
	const QVec3 p1 = GetPosition(Indexes[TriIndex * 3 + 1]);
	const QVec3 p2 = GetPosition(Indexes[TriIndex * 3 + 2]);

	const QVec3 Normal = (p2 - p0) ^ (p1 - p0);

	// Didn't find matching edge. Add edge constraint.
	//EdgeQuadrics[ EdgeIndex ] = FEdgeQuadric( GetPosition( VertIndex0 ), GetPosition( VertIndex1 ), Normal, Weight );
	EdgeQuadrics[EdgeIndex] = FEdgeQuadric(GetPosition(VertIndex0), GetPosition(VertIndex1), Weight);
	EdgeQuadricsValid[EdgeIndex] = true;
}

void FMeshSimplifier1::GatherAdjTris(const FVector3f& Position, uint32_t Flag, std::vector<uint32_t>& AdjTris, int32_t& VertDegree, uint32_t& FlagsUnion)
{
	struct FWedgeVert
	{
		uint32_t VertIndex;
		uint32_t AdjTriIndex;
	};
	std::vector<FWedgeVert> WedgeVerts;

	ForAllCorners(Position,
		[this, &AdjTris, &WedgeVerts, &VertDegree, Flag, &FlagsUnion](uint32_t Corner)
		{
			VertDegree++;

			{
				uint8_t& __restrict CornerFlag = CornerFlags[Corner];
				FlagsUnion |= CornerFlag;
				CornerFlag |= Flag;
			}

			uint32_t TriIndex = Corner / 3;
			uint32_t AdjTriIndex;
			bool bNewTri = true;

			uint8_t& __restrict FirstCornerFlag = CornerFlags[TriIndex * 3];
			if ((FirstCornerFlag & AdjTriMask) == 0)
			{
				FirstCornerFlag |= AdjTriMask;
				AdjTriIndex = AdjTris.size();
				AdjTris.push_back(TriIndex);
				WedgeDisjointSet.AddDefaulted();
			}
			else
			{
				// Should only happen 2 times per collapse on average
				//AdjTriIndex = AdjTris.Find(TriIndex);
				for (int32_t i = 0; i < AdjTris.size(); i++)
				{
					if (AdjTris[i] == TriIndex)
					{
						AdjTriIndex = i;
						break;
					}
				}
				bNewTri = false;
			}

			uint32_t VertIndex = Indexes[Corner];
			uint32_t OtherAdjTriIndex = ~0u;
			for (FWedgeVert& WedgeVert : WedgeVerts)
			{
				if (VertIndex == WedgeVert.VertIndex)
				{
					OtherAdjTriIndex = WedgeVert.AdjTriIndex;
					break;
				}
			}
			if (OtherAdjTriIndex == ~0u)
			{
				WedgeVerts.push_back({ VertIndex, AdjTriIndex });
			}
			else
			{
				if (bNewTri)
					WedgeDisjointSet.UnionSequential(AdjTriIndex, OtherAdjTriIndex);
				else
					WedgeDisjointSet.Union(AdjTriIndex, OtherAdjTriIndex);
			}
		});
}

float FMeshSimplifier1::EvaluateMerge(const FVector3f& Position0, const FVector3f& Position1, bool bMoveVerts)
{
	//check( Position0 != Position1 );
	if (Position0 == Position1)
		return 0.0f;

	// Find unique adjacent triangles
	std::vector<uint32_t> AdjTris;

	WedgeDisjointSet.Reset();

	int32_t VertDegree = 0;

	uint32_t FlagsUnion0 = 0;
	uint32_t FlagsUnion1 = 0;

	GatherAdjTris(Position0, 1, AdjTris, VertDegree, FlagsUnion0);
	GatherAdjTris(Position1, 2, AdjTris, VertDegree, FlagsUnion1);

	if (VertDegree == 0)
	{
		return 0.0f;
	}

	bool bLocked0 = FlagsUnion0 & LockedVertMask;
	bool bLocked1 = FlagsUnion1 & LockedVertMask;

	float Penalty = 0.0f;

	if (VertDegree > DegreeLimit)
		Penalty += DegreePenalty * (VertDegree - DegreeLimit);

	std::vector<uint32_t>	WedgeIDs;
	std::vector<uint8_t>	WedgeQuadrics;

	const unsigned __int64 QuadricSize = sizeof(FQuadricAttr) + NumAttributes * 4 * sizeof(QScalar);

	auto GetWedgeQuadric =
		[&WedgeQuadrics, QuadricSize](int32_t WedgeIndex) -> FQuadricAttr&
		{
			return *reinterpret_cast<FQuadricAttr*>(&WedgeQuadrics[WedgeIndex * QuadricSize]);
		};

	for (uint32_t AdjTriIndex = 0, Num = AdjTris.size(); AdjTriIndex < Num; AdjTriIndex++)
	{
		uint32_t TriIndex = AdjTris[AdjTriIndex];

		FQuadricAttr& __restrict TriQuadric = GetTriQuadric(TriIndex);

		uint32_t WedgeID = WedgeDisjointSet.Find(AdjTriIndex);
		//int32_t WedgeIndex = WedgeIDs.Find(WedgeID);
		int32_t WedgeIndex = -1;
		for (uint32_t i = 0; i < WedgeIDs.size(); i++)
		{
			if (WedgeIDs[i] == WedgeID)
			{
				WedgeIndex = i;
				break;
			}
		}
		if (WedgeIndex != -1)
		{
			FQuadricAttr& __restrict WedgeQuadric = GetWedgeQuadric(WedgeIndex);
			uint32_t VertIndex0 = Indexes[TriIndex * 3];
			WedgeQuadric.Add(TriQuadric, GetPosition(VertIndex0) - Position0, GetAttributes(VertIndex0), AttributeWeights, NumAttributes);
		}
		else
		{
			WedgeIndex = WedgeIDs.size();
			WedgeIDs.push_back(WedgeID);
			WedgeQuadrics.insert(WedgeQuadrics.end(), QuadricSize, 0);//WedgeQuadrics.AddUninitialized(QuadricSize);

			FQuadricAttr& __restrict WedgeQuadric = GetWedgeQuadric(WedgeIndex);

			std::memcpy(&WedgeQuadric, &TriQuadric, QuadricSize);
			uint32_t VertIndex0 = Indexes[TriIndex * 3];
			WedgeQuadric.Rebase(GetPosition(VertIndex0) - Position0, GetAttributes(VertIndex0), AttributeWeights, NumAttributes);
		}
	}

	FQuadricAttrOptimizer QuadricOptimizer;
	for (int32_t WedgeIndex = 0, Num = WedgeIDs.size(); WedgeIndex < Num; WedgeIndex++)
	{
		QuadricOptimizer.AddQuadric(GetWedgeQuadric(WedgeIndex), NumAttributes);
	}

	FVector3f	BoundsMin = { XSM_MAX_flt,  XSM_MAX_flt,  XSM_MAX_flt };
	FVector3f	BoundsMax = { -XSM_MAX_flt, -XSM_MAX_flt, -XSM_MAX_flt };

	FQuadric EdgeQuadric;
	EdgeQuadric.Zero();

	for (uint32_t TriIndex : AdjTris)
	{
		for (uint32_t CornerIndex = 0; CornerIndex < 3; CornerIndex++)
		{
			uint32_t Corner = TriIndex * 3 + CornerIndex;

			const FVector3f& Position = GetPosition(Indexes[Corner]);

			BoundsMin = FVector3f::Min(BoundsMin, Position);
			BoundsMax = FVector3f::Max(BoundsMax, Position);

			if (EdgeQuadricsValid[Corner])
			{
				// Only if edge is part of this pair
				uint32_t EdgeFlags;
				EdgeFlags = CornerFlags[Corner];
				EdgeFlags |= CornerFlags[TriIndex * 3 + ((1 << CornerIndex) & 3)];
				if (EdgeFlags & MergeMask)
				{
					EdgeQuadric.Add(EdgeQuadrics[Corner], GetPosition(Indexes[Corner]) - Position0);
				}
			}
		}
	}

	QuadricOptimizer.AddQuadric(EdgeQuadric);

	auto IsValidPosition =
		[this, &AdjTris, &BoundsMin, &BoundsMax](const FVector3f& Position) -> bool
		{
			// Limit position to be near the neighborhood bounds
			if (ComputeSquaredDistanceFromBoxToPoint(BoundsMin, BoundsMax, Position) > (BoundsMax - BoundsMin).SizeSquared() * 4.0f)
				return false;

			for (uint32_t TriIndex : AdjTris)
			{
				if (TriWillInvert(TriIndex, Position))
					return false;
			}

			return true;
		};

	FVector3f NewPosition;
	{
		if (bLocked0 && bLocked1)
			Penalty += LockPenalty;

		// find position
		if (bLocked0 && !bLocked1)
		{
			NewPosition = Position0;

			if (!IsValidPosition(NewPosition))
				Penalty += InversionPenalty;
		}
		else if (bLocked1 && !bLocked0)
		{
			NewPosition = Position1;

			if (!IsValidPosition(NewPosition))
				Penalty += InversionPenalty;
		}
		else
		{
			bool bIsValid = QuadricOptimizer.OptimizeVolume(NewPosition);
			NewPosition += Position0;

			if (bIsValid)
				bIsValid = IsValidPosition(NewPosition);

			if (!bIsValid)
			{
				bIsValid = QuadricOptimizer.Optimize(NewPosition);

				NewPosition += Position0;

				if (bIsValid)
					bIsValid = IsValidPosition(NewPosition);
			}

			if (!bIsValid)
			{
				// Try a point on the edge.
				bIsValid = QuadricOptimizer.OptimizeLinear(FVector3f(0,0,0), Position1 - Position0, NewPosition);
				NewPosition += Position0;

				if (bIsValid)
					bIsValid = IsValidPosition(NewPosition);
			}

			if (!bIsValid)
			{
				// Couldn't find optimal so choose middle
				NewPosition = (Position0 + Position1) * 0.5f;

				if (!IsValidPosition(NewPosition))
					Penalty += InversionPenalty;
			}
		}
	}

	int32_t NumWedges = WedgeIDs.size();
	WedgeAttributes.clear();
	WedgeAttributes.resize(NumWedges * NumAttributes);

	float Error = 0.0f;

	float EdgeError = EdgeQuadric.Evaluate(NewPosition - Position0);

	float SurfaceArea = 0.0f;

	for (int32_t WedgeIndex = 0; WedgeIndex < NumWedges; WedgeIndex++)
	{
		float* __restrict NewAttributes = &WedgeAttributes[WedgeIndex * NumAttributes];

		FQuadricAttr& __restrict WedgeQuadric = GetWedgeQuadric(WedgeIndex);
		if (WedgeQuadric.a > 1e-8)
		{
			// calculate vert attributes from the new position
			float WedgeError = WedgeQuadric.CalcAttributesAndEvaluate(NewPosition - Position0, NewAttributes, AttributeWeights, NumAttributes);

			// Correct after eval. Normal length is unimportant for error but can bias the calculation.
			if (CorrectAttributes != nullptr)
				CorrectAttributes(NewAttributes);

			if (bLimitErrorToSurfaceArea)
				WedgeError = std::min(WedgeError, (float)WedgeQuadric.a);

			Error += WedgeError;
		}
		else
		{
			for (uint32_t i = 0; i < NumAttributes; i++)
			{
				NewAttributes[i] = 0.0f;
			}
		}

		SurfaceArea += WedgeQuadric.a;
	}

	Error += EdgeError;

	bool bIsDisjoint = AdjTris.size() == 1 || (AdjTris.size() == 2 && VertDegree == 4);

	if (bLimitErrorToSurfaceArea)
	{
		// Limit error to be no greater than the size of the triangles it could affect.
		Error = std::min(Error, SurfaceArea);

		// Collapsing with completely remove this surface area. The position merged to is irrelevant.
		if (bIsDisjoint)
			Error = SurfaceArea;
	}

	if (bMoveVerts)
	{
		BeginMovePosition(Position0);
		BeginMovePosition(Position1);

		for (uint32_t AdjTriIndex = 0, Num = AdjTris.size(); AdjTriIndex < Num; AdjTriIndex++)
		{
			int32_t WedgeIndex = -1;//WedgeIDs.Find(WedgeDisjointSet[AdjTriIndex]);
			for (uint32_t i = 0; i < WedgeIDs.size(); i++)
			{
				if (WedgeIDs[i] == WedgeDisjointSet[AdjTriIndex])
				{
					WedgeIndex = i;
					break;
				}
			}

			for (uint32_t CornerIndex = 0; CornerIndex < 3; CornerIndex++)
			{
				uint32_t Corner = AdjTris[AdjTriIndex] * 3 + CornerIndex;
				uint32_t VertIndex = Indexes[Corner];

				FVector3f& OldPosition = GetPosition(VertIndex);
				if (OldPosition == Position0 ||
					OldPosition == Position1)
				{
					OldPosition = NewPosition;

					// Only use attributes if we calculated them.
					if (GetWedgeQuadric(WedgeIndex).a > 1e-8)
					{
						float* __restrict NewAttributes = &WedgeAttributes[WedgeIndex * NumAttributes];
						float* __restrict OldAttributes = GetAttributes(VertIndex);

						for (uint32_t i = 0; i < NumAttributes; i++)
						{
							OldAttributes[i] = NewAttributes[i];
						}
					}

					// If either position was locked then lock the new verts.
					if (bLocked0 || bLocked1)
						CornerFlags[Corner] |= LockedVertMask;
				}
			}
		}

		for (uint32_t PairIndex : MovedPairs)
		{
			FPair& Pair = Pairs[PairIndex];

			if (Pair.Position0 == Position0 ||
				Pair.Position0 == Position1)
			{
				Pair.Position0 = NewPosition;
			}

			if (Pair.Position1 == Position0 ||
				Pair.Position1 == Position1)
			{
				Pair.Position1 = NewPosition;
			}
		}

		EndMovePositions();

		std::set<uint32_t> AdjVerts;
		for (uint32_t TriIndex : AdjTris)
		{
			for (uint32_t CornerIndex = 0; CornerIndex < 3; CornerIndex++)
			{
				AdjVerts.insert(Indexes[TriIndex * 3 + CornerIndex]);//AdjVerts.AddUnique(Indexes[TriIndex * 3 + CornerIndex]);
			}
		}

		// Reevaluate all pairs touching an adjacent tri.
		// Duplicate pairs have already been removed.
		for (uint32_t VertIndex : AdjVerts)
		{
			const FVector3f& Position = GetPosition(VertIndex);

			ForAllPairs(Position,
				[this](uint32_t PairIndex)
				{
					// IsPresent used to mark Pairs we have already added to the list.
					if (PairHeap.IsPresent(PairIndex))
					{
						PairHeap.Remove(PairIndex);
						ReevaluatePairs.push_back(PairIndex);//ReevaluatePairs.Add(PairIndex);
					}
				});
		}

		for (uint32_t TriIndex : AdjTris)
		{
			int32_t MaterialIndex = MaterialIndexes[TriIndex] & 0xffffff;
			//if (!PerMaterialDeltas.IsValidIndex(MaterialIndex))
			if (MaterialIndex >= PerMaterialDeltas.size())
			{
				//PerMaterialDeltas.SetNumZeroed(MaterialIndex + 1);
				PerMaterialDeltas.insert(PerMaterialDeltas.end(), MaterialIndex + 1 - PerMaterialDeltas.size(), { 0 });
			}

			auto& Delta = PerMaterialDeltas[MaterialIndex];

			Delta.SurfaceArea -= GetTriQuadric(TriIndex).a;
			Delta.NumTris--;
			Delta.NumDisjoint -= bIsDisjoint ? 1 : 0;

			FixUpTri(TriIndex);

			if (!TriRemoved[TriIndex])
			{
				Delta.SurfaceArea += GetTriQuadric(TriIndex).a;
				Delta.NumTris++;
			}
		}
	}
	else
	{
		Error += Penalty;
	}

	for (uint32_t TriIndex : AdjTris)
	{
		for (uint32_t CornerIndex = 0; CornerIndex < 3; CornerIndex++)
		{
			uint32_t Corner = TriIndex * 3 + CornerIndex;

			// Must be separated from FixUpTri loop because relies on correct indexing
			if (bMoveVerts)
				CalcEdgeQuadric(Corner);

			// Clear flags
			CornerFlags[Corner] &= ~(MergeMask | AdjTriMask);
		}
	}

	return Error;
}

void FMeshSimplifier1::BeginMovePosition(const FVector3f& Position)
{
	uint32_t Hash = HashPosition(Position);

	ForAllVerts(Position,
		[this, Hash](uint32_t VertIndex)
		{
			// Safe to remove while iterating.
			VertHash.Remove(Hash, VertIndex);
			MovedVerts.push_back(VertIndex);
		});

	ForAllCorners(Position,
		[this, Hash](uint32_t Corner)
		{
			// Safe to remove while iterating.
			CornerHash.Remove(Hash, Corner);
			MovedCorners.push_back(Corner);
		});

	ForAllPairs(Position,
		[this](uint32_t PairIndex)
		{
			// Safe to remove while iterating.
			PairHash0.Remove(HashPosition(Pairs[PairIndex].Position0), PairIndex);
			PairHash1.Remove(HashPosition(Pairs[PairIndex].Position1), PairIndex);
			MovedPairs.push_back(PairIndex);
		});
}

void FMeshSimplifier1::EndMovePositions()
{
	for (uint32_t VertIndex : MovedVerts)
	{
		VertHash.Add(HashPosition(GetPosition(VertIndex)), VertIndex);
	}

	for (uint32_t Corner : MovedCorners)
	{
		CornerHash.Add(HashPosition(GetPosition(Indexes[Corner])), Corner);
	}

	for (uint32_t PairIndex : MovedPairs)
	{
		FPair& Pair = Pairs[PairIndex];

		if (Pair.Position0 == Pair.Position1 || !AddUniquePair(Pair, PairIndex))
		{
			// Found invalid or duplicate pair
			PairHeap.Remove(PairIndex);
		}
	}

	MovedVerts.clear();
	MovedCorners.clear();
	MovedPairs.clear();
}

uint32_t FMeshSimplifier1::CornerIndexMoved(uint32_t TriIndex) const
{
	uint32_t IndexMoved = 3;
	for (uint32_t CornerIndex = 0; CornerIndex < 3; CornerIndex++)
	{
		uint32_t Corner = TriIndex * 3 + CornerIndex;

		if (CornerFlags[Corner] & MergeMask)
		{
			if (IndexMoved == 3)
				IndexMoved = CornerIndex;
			else
				IndexMoved = 4;
		}
	}
	return IndexMoved;
}

bool FMeshSimplifier1::TriWillInvert(uint32_t TriIndex, const FVector3f& NewPosition) const
{
	uint32_t IndexMoved = CornerIndexMoved(TriIndex);

	if (IndexMoved < 3)
	{
		uint32_t Corner = TriIndex * 3 + IndexMoved;

		const FVector3f& p0 = GetPosition(Indexes[Corner]);
		const FVector3f& p1 = GetPosition(Indexes[Cycle3(Corner)]);
		const FVector3f& p2 = GetPosition(Indexes[Cycle3(Corner, 2)]);

		const FVector3f d21 = p2 - p1;
		const FVector3f d01 = p0 - p1;
		const FVector3f dp1 = NewPosition - p1;

#if 1
		FVector3f n0 = d01 ^ d21;
		FVector3f n1 = dp1 ^ d21;

		return (n0 | n1) < 0.0f;
#else
		FVector3f n = d21 ^ d01 ^ d21;
		//n.Normalize();

		float InversionThreshold = 0.0f;
		return (dp1 | n) < InversionThreshold * (d01 | n);
#endif
	}

	return false;
}

void FMeshSimplifier1::FixUpTri(uint32_t TriIndex)
{
	const FVector3f& p0 = GetPosition(Indexes[TriIndex * 3 + 0]);
	const FVector3f& p1 = GetPosition(Indexes[TriIndex * 3 + 1]);
	const FVector3f& p2 = GetPosition(Indexes[TriIndex * 3 + 2]);

	bool bRemoveTri = CornerFlags[TriIndex * 3] & RemoveTriMask;

	if (!bRemoveTri)
	{
		// Remove degenerates
		bRemoveTri =
			p0 == p1 ||
			p1 == p2 ||
			p2 == p0;
	}

	if (!bRemoveTri)
	{
		for (uint32_t k = 0; k < 3; k++)
		{
			RemoveDuplicateVerts(TriIndex * 3 + k);
		}

		bRemoveTri = IsDuplicateTri(TriIndex);
	}

	if (bRemoveTri)
	{
		TriRemoved[TriIndex] = true;
		RemainingNumTris--;

		// Remove references to tri
		for (uint32_t k = 0; k < 3; k++)
		{
			uint32_t Corner = TriIndex * 3 + k;
			uint32_t VertIndex = Indexes[Corner];
			uint32_t Hash = HashPosition(GetPosition(VertIndex));

			CornerHash.Remove(Hash, Corner);
			EdgeQuadricsValid[Corner] = false;

			SetVertIndex(Corner, ~0u);
		}
	}
	else
	{
		CalcTriQuadric(TriIndex);
	}
}

bool FMeshSimplifier1::IsDuplicateTri(uint32_t TriIndex) const
{
	uint32_t i0 = Indexes[TriIndex * 3 + 0];
	uint32_t i1 = Indexes[TriIndex * 3 + 1];
	uint32_t i2 = Indexes[TriIndex * 3 + 2];

	uint32_t Hash = HashPosition(GetPosition(i0));
	for (uint32_t Corner = CornerHash.First(Hash); CornerHash.IsValid(Corner); Corner = CornerHash.Next(Corner))
	{
		if (Corner != TriIndex * 3 &&
			i0 == Indexes[Corner] &&
			i1 == Indexes[Cycle3(Corner)] &&
			i2 == Indexes[Cycle3(Corner, 2)])
		{
			return true;
		}
	}

	return false;
}

void FMeshSimplifier1::SetVertIndex(uint32_t Corner, uint32_t NewVertIndex)
{
	uint32_t& VertIndex = Indexes[Corner];
	if (VertIndex == NewVertIndex)
		return;

	uint32_t RefCount = --VertRefCount[VertIndex];
	if (RefCount == 0)
	{
		VertHash.Remove(HashPosition(GetPosition(VertIndex)), VertIndex);
		RemainingNumVerts--;
	}

	VertIndex = NewVertIndex;
	if (VertIndex != ~0u)
	{
		VertRefCount[VertIndex]++;
	}
}

// Remove identical valued verts.
void FMeshSimplifier1::RemoveDuplicateVerts(uint32_t Corner)
{
	uint32_t& VertIndex = Indexes[Corner];
	float* VertData = &Verts[(3 + NumAttributes) * VertIndex];

	uint32_t Hash = HashPosition(GetPosition(VertIndex));
	for (uint32_t OtherVertIndex = VertHash.First(Hash); VertHash.IsValid(OtherVertIndex); OtherVertIndex = VertHash.Next(OtherVertIndex))
	{
		if (VertIndex == OtherVertIndex)
			break;

		float* OtherVertData = &Verts[(3 + NumAttributes) * OtherVertIndex];
		if (std::memcmp(VertData, OtherVertData, (3 + NumAttributes) * sizeof(float)) == 0)
		{
			// First entry in hashtable for this vert value is authoritative.
			SetVertIndex(Corner, OtherVertIndex);
			break;
		}
	}
}

float FMeshSimplifier1::Simplify(
	uint32_t TargetNumVerts, uint32_t TargetNumTris, float TargetError,
	uint32_t LimitNumVerts, uint32_t LimitNumTris, float LimitError)
{
	const unsigned __int64 QuadricSize = sizeof(FQuadricAttr) + NumAttributes * 4 * sizeof(QScalar);

	TriQuadrics.resize(NumTris * QuadricSize);
	for (uint32_t TriIndex = 0; TriIndex < NumTris; TriIndex++)
	{
		FixUpTri(TriIndex);
	}

	for (uint32_t i = 0; i < NumIndexes; i++)
	{
		CalcEdgeQuadric(i);
	}

	// Initialize heap
	PairHeap.Resize(Pairs.size(), Pairs.size());

	for (uint32_t PairIndex = 0, Num = Pairs.size(); PairIndex < Num; PairIndex++)
	{
		FPair& Pair = Pairs[PairIndex];

		float MergeError = EvaluateMerge(Pair.Position0, Pair.Position1, false);
		PairHeap.Add(MergeError, PairIndex);
	}

	float MaxError = 0.0f;

	while (PairHeap.Num() > 0)
	{
		uint32_t PrevNumVerts = RemainingNumVerts;
		uint32_t PrevNumTris = RemainingNumTris;

		if (PairHeap.GetKey(PairHeap.Top()) > LimitError)
			break;

		{
			uint32_t PairIndex = PairHeap.Top();
			PairHeap.Pop();

			FPair& Pair = Pairs[PairIndex];

			PairHash0.Remove(HashPosition(Pair.Position0), PairIndex);
			PairHash1.Remove(HashPosition(Pair.Position1), PairIndex);

			float MergeError = EvaluateMerge(Pair.Position0, Pair.Position1, true);
			MaxError = std::max(MaxError, MergeError);
		}

		if (RemainingNumVerts <= TargetNumVerts &&
			RemainingNumTris <= TargetNumTris &&
			MaxError >= TargetError)
		{
			break;
		}

		if (RemainingNumVerts <= LimitNumVerts ||
			RemainingNumTris <= LimitNumTris ||
			MaxError >= LimitError)
		{
			break;
		}

		for (uint32_t PairIndex : ReevaluatePairs)
		{
			FPair& Pair = Pairs[PairIndex];

			float MergeError = EvaluateMerge(Pair.Position0, Pair.Position1, false);
			PairHeap.Add(MergeError, PairIndex);
		}
		ReevaluatePairs.clear();
	}

	return MaxError;
}

void FMeshSimplifier1::Compact()
{
	uint32_t OutputVertIndex = 0;
	for (uint32_t VertIndex = 0; VertIndex < NumVerts; VertIndex++)
	{
		if (VertRefCount[VertIndex] > 0)
		{
			if (VertIndex != OutputVertIndex)
			{
				float* SrcData = &Verts[(3 + NumAttributes) * VertIndex];
				float* DstData = &Verts[(3 + NumAttributes) * OutputVertIndex];
				std::memcpy(DstData, SrcData, (3 + NumAttributes) * sizeof(float));
			}

			// Reuse VertRefCount as OutputVertIndex
			VertRefCount[VertIndex] = OutputVertIndex++;
		}
	}

	uint32_t OutputTriIndex = 0;
	for (uint32_t TriIndex = 0; TriIndex < NumTris; TriIndex++)
	{
		if (!TriRemoved[TriIndex])
		{
			for (uint32_t k = 0; k < 3; k++)
			{
				// Reuse VertRefCount as OutputVertIndex
				uint32_t VertIndex = Indexes[TriIndex * 3 + k];
				uint32_t OutVertIndex = VertRefCount[VertIndex];
				Indexes[OutputTriIndex * 3 + k] = OutVertIndex;
			}

			MaterialIndexes[OutputTriIndex++] = MaterialIndexes[TriIndex];
		}
	}
}

void CorrectAttributes(float* Attributes)
{
	FVector3f& Normal = *reinterpret_cast<FVector3f*>(Attributes);
	Normal.Normalize();
}

bool SimplyMesh(const std::vector<FVector3f>& InPositions, float PercentTriangles, float PercentVertices, std::vector<FVector3f>& OutPositions, std::vector<FVector3f>& OutNormals, std::vector<uint32_t>& OutIndices)
{
	uint32_t NumVertices = (uint32_t)InPositions.size();
	std::vector<uint32_t> InIndices;
	InIndices.resize(NumVertices);
	for (uint32_t i = 0; i < NumVertices; i++)
		InIndices[i] = i;

	FOverlappingCorners OverlappingCorners(InPositions, InIndices);

	std::vector<FVertSimp> Verts;
	std::vector<uint32_t> Indexes;
	int32_t NumTriangles = NumVertices / 3;
	int32_t NumWedges = NumTriangles * 3;

	std::map<int32_t, int32_t> VertsMap;

	float SurfaceArea = 0.0f;
	int32_t WedgeIndex = 0;
	for (int32_t i = 0; i < NumTriangles; i++)
	{
		FVector3f CornerPositions[3];
		for (int32_t TriVert = 0; TriVert < 3; ++TriVert)
		{
			CornerPositions[TriVert] = InPositions[i * 3 + TriVert];
		}

		if (PointsEqual(CornerPositions[0], CornerPositions[1]) ||
			PointsEqual(CornerPositions[0], CornerPositions[2]) ||
			PointsEqual(CornerPositions[1], CornerPositions[2]))
		{
			WedgeIndex += 3;
			continue;
		}

		FVector3f TriNormal;
		{
			const FVector3f Edge21 = CornerPositions[1] - CornerPositions[2];
			const FVector3f Edge20 = CornerPositions[0] - CornerPositions[1];
			TriNormal = (Edge21 ^ Edge20).GetSafeNormal();
			TriNormal.Normalize();
		}

		int32_t VertexIndices[3];
		for (int32_t TriVert = 0; TriVert < 3; ++TriVert, ++WedgeIndex)
		{
			FVertSimp NewVert;
			NewVert.Position = CornerPositions[TriVert];
			NewVert.Normal = TriNormal;

			const std::vector<int32_t>& DupVerts = OverlappingCorners.FindIfOverlapping(WedgeIndex);

			int32_t Index = -1;
			for (int32_t k = 0; k < DupVerts.size(); k++)
			{
				if (DupVerts[k] >= WedgeIndex)
				{
					break;
				}

				std::map<int32_t, int32_t>::iterator Iter = VertsMap.find(DupVerts[k]);
				if (Iter != VertsMap.end())
				{
					FVertSimp& FoundVert = Verts[Iter->second];
					if (NewVert.Equals(FoundVert))
					{
						Index = Iter->second;
						break;
					}
				}
			}
			if (Index == -1)
			{
				Index = (int32_t)Verts.size();
				Verts.push_back(NewVert);
				VertsMap.insert(std::pair<int32_t, int32_t>(WedgeIndex, Index));
			}
			VertexIndices[TriVert] = Index;
		}
		if (VertexIndices[0] == VertexIndices[1] ||
			VertexIndices[1] == VertexIndices[2] ||
			VertexIndices[0] == VertexIndices[2])
		{
			continue;
		}

		{
			FVector3f Edge01 = CornerPositions[1] - CornerPositions[0];
			FVector3f Edge20 = CornerPositions[0] - CornerPositions[2];

			float TriArea = 0.5f * (Edge01 ^ Edge20).Size();
			SurfaceArea += TriArea;
		}

		Indexes.push_back(VertexIndices[0]);
		Indexes.push_back(VertexIndices[1]);
		Indexes.push_back(VertexIndices[2]);
	}

	int32_t NumVerts = (int32_t)Verts.size();
	int32_t NumIndexes = (int32_t)Indexes.size();
	int32_t NumTris = NumIndexes / 3;

	
	int32_t TargetNumTris = (int32_t)(NumTris * PercentTriangles);
	int32_t TargetNumVerts = (int32_t)(NumVerts * PercentVertices);

	TargetNumTris = std::max(TargetNumTris, 2);
	TargetNumVerts = std::max(TargetNumVerts, 4);

	{
		if (TargetNumVerts < NumVerts || TargetNumTris < NumTris)
		{
			const uint32_t NumAttributes = (sizeof(FVertSimp) - sizeof(FVector3f)) / sizeof(float);
			float AttributeWeights[NumAttributes] =
			{
				16.0f, 16.0f, 16.0f// Normal
			};

			std::vector<int32_t> MaterialIndexes;
			MaterialIndexes.insert(MaterialIndexes.end(), NumTris, 0);

			FMeshSimplifier1 Simplifier((float*)Verts.data(), Verts.size(), Indexes.data(), Indexes.size(), MaterialIndexes.data(), NumAttributes);

			Simplifier.SetAttributeWeights(AttributeWeights);
			Simplifier.SetCorrectAttributes(CorrectAttributes);
			Simplifier.SetEdgeWeight(512.0f);
			Simplifier.SetLimitErrorToSurfaceArea(false);

			Simplifier.DegreePenalty = 100.0f;
			Simplifier.InversionPenalty = 1000000.0f;

			float MaxErrorSqr = Simplifier.Simplify(TargetNumVerts, TargetNumTris, 0.0f, 4, 2, XSM_MAX_flt);

			if (Simplifier.GetRemainingNumVerts() == 0 || Simplifier.GetRemainingNumTris() == 0)
			{
				return false;
			}

			Simplifier.Compact();

			//Verts.SetNum(Simplifier.GetRemainingNumVerts());
			//Indexes.SetNum(Simplifier.GetRemainingNumTris() * 3);

			NumVerts = Simplifier.GetRemainingNumVerts();
			NumTris = Simplifier.GetRemainingNumTris();
			NumIndexes = NumTris * 3;
		}
		else
		{
			return false;
		}
	}

	OutPositions.resize(NumVerts);
	OutNormals.resize(NumVerts);
	for (int32_t i = 0; i < NumVerts; i++)
	{
		OutPositions[i] = Verts[i].Position;
		OutNormals[i] = Verts[i].Normal;
	}
	OutIndices.resize(NumIndexes);
	for (int32_t i = 0; i < NumIndexes; i++)
	{
		OutIndices[i] = Indexes[i];
	}

	return true;
}
}

bool XSPSimplifyMesh(const std::vector<float>& InPositions, float PercentTriangles, float PercentVertices, std::vector<float>& OutPositions, std::vector<float>& OutNormals, std::vector<uint32_t>& OutIndices)
{
	std::vector<XspMeshSimp::FVector3f> Positions;
	std::vector<XspMeshSimp::FVector3f> SimpPositions, SimpNormals;
	size_t num = InPositions.size() / 3;
	if (num < 10)
		return false;
	Positions.resize(num);
	for (size_t i = 0; i < num; i++)
	{
		Positions[i].Set(InPositions[i * 3 + 0], InPositions[i * 3 + 1], InPositions[i * 3 + 2]);
	}
	bool bSuccess = XspMeshSimp::SimplyMesh(Positions, PercentTriangles, PercentVertices, SimpPositions, SimpNormals, OutIndices);
	if (!bSuccess)
		return false;

	size_t num1 = SimpPositions.size();
	if (num1 < 3)
		return false;
	OutPositions.resize(num1*3);
	OutNormals.resize(num1*3);
	for (size_t i = 0; i < num1; i++)
	{
		OutPositions[i * 3 + 0] = SimpPositions[i].X;
		OutPositions[i * 3 + 1] = SimpPositions[i].Y;
		OutPositions[i * 3 + 2] = SimpPositions[i].Z;
		OutNormals[i * 3 + 0] = SimpNormals[i].X;
		OutNormals[i * 3 + 1] = SimpNormals[i].Y;
		OutNormals[i * 3 + 2] = SimpNormals[i].Z;
	}

	return true;
}