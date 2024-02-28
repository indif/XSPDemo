#pragma once
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <malloc.h>

namespace XspMeshSimp
{

#define XSM_SMALL_NUMBER			(1.e-8f)
#define XSM_KINDA_SMALL_NUMBER	(1.e-4f)
#define XSM_BIG_NUMBER			(3.4e+38f)
#define XSM_THRESH_POINTS_ARE_SAME			(0.00002f)	/* Two points are same if within this distance */
#define XSM_THRESH_NORMALS_ARE_SAME			(0.00002f)	/* Two normal points are same if within this distance */
#define XSM_MAX_flt			(3.402823466e+38F)

    /**
     * XSM_FMemory_Alloca/alloca implementation. This can't be a function, even FORCEINLINE'd because there's no guarantee that
     * the memory returned in a function will stick around for the caller to use.
     */
#if PLATFORM_USES_MICROSOFT_LIBC_FUNCTIONS
#define __FMemory_Alloca_Func _alloca
#else
#define __FMemory_Alloca_Func alloca
#endif

#define XSM_FMemory_Alloca(Size) ((Size==0) ? 0 : (void*)(((int32_t*)__FMemory_Alloca_Func(Size + 15) + 15) & ~15))


    template< class T >
    static constexpr T Clamp(const T X, const T Min, const T Max)
    {
        return (X < Min) ? Min : (X < Max) ? X : Max;
    }

    struct FVector3f
    {
        float X, Y, Z;

        FVector3f()
            : X(0), Y(0), Z(0)
        {
        }

        FVector3f(float InX, float InY, float InZ)
            : X(InX), Y(InY), Z(InZ)
        {
        }

        void Set(float InX, float InY, float InZ)
        {
            X = InX;
            Y = InY;
            Z = InZ;
        }

        FVector3f operator^(const FVector3f& V) const
        {
            return FVector3f
            (
                Y * V.Z - Z * V.Y,
                Z * V.X - X * V.Z,
                X * V.Y - Y * V.X
            );
        }

        FVector3f GetSafeNormal() const
        {
            const float SquareSum = X * X + Y * Y + Z * Z;

            // Not sure if it's safe to add tolerance in there. Might introduce too many errors
            if (SquareSum == 1.f)
            {
                return *this;
            }
            else if (SquareSum < XSM_SMALL_NUMBER)
            {
                return FVector3f(0, 0, 0);
            }
            const float Scale = 1 / std::sqrt(SquareSum);
            return FVector3f(X * Scale, Y * Scale, Z * Scale);
        }

        bool Normalize()
        {
            const float SquareSum = X * X + Y * Y + Z * Z;
            if (SquareSum > XSM_SMALL_NUMBER)
            {
                const float Scale = 1 / std::sqrt(SquareSum);
                X *= Scale; Y *= Scale; Z *= Scale;
                return true;
            }
            return false;
        }

        float Size() const
        {
            return std::sqrt(X * X + Y * Y + Z * Z);
        }

        FVector3f operator-(const FVector3f& V) const
        {
            return FVector3f(X - V.X, Y - V.Y, Z - V.Z);
        }

        bool operator==(const FVector3f& V) const
        {
            return X == V.X && Y == V.Y && Z == V.Z;
        }

        float SizeSquared() const
        {
            return X * X + Y * Y + Z * Z;
        }

        FVector3f operator+(const FVector3f& V) const
        {
            return FVector3f(X + V.X, Y + V.Y, Z + V.Z);
        }

        FVector3f  operator+=(const FVector3f& V)
        {
            X += V.X; Y += V.Y; Z += V.Z;
            return *this;
        }

        FVector3f operator*(float Scale) const
        {
            return FVector3f(X * Scale, Y * Scale, Z * Scale);
        }

        float operator|(const FVector3f& V) const
        {
            return X * V.X + Y * V.Y + Z * V.Z;
        }

        static FVector3f Min(const FVector3f& A, const FVector3f& B)
        {
            return FVector3f(
                std::min(A.X, B.X),
                std::min(A.Y, B.Y),
                std::min(A.Z, B.Z)
            );
        }

        static FVector3f Max(const FVector3f& A, const FVector3f& B)
        {
            return FVector3f(
                std::max(A.X, B.X),
                std::max(A.Y, B.Y),
                std::max(A.Z, B.Z)
            );
        }
    };

    inline float Square(float A)
    {
        return A * A;
    }

    float ComputeSquaredDistanceFromBoxToPoint(const FVector3f& Mins, const FVector3f& Maxs, const FVector3f& Point);


    int32_t GetLeadingBit(uint32_t value);

    uint64_t RoundUpToPowerOfTwo(uint32_t value);

    uint32_t HashPosition(const FVector3f& Position);

    class FHashTable
    {
    public:
        FHashTable(uint32_t InHashSize = 1024, uint32_t InIndexSize = 0);
        FHashTable(const FHashTable& Other);
        ~FHashTable();

        void			Clear();
        void			Clear(uint32_t InHashSize, uint32_t InIndexSize = 0);
        void			Free();
        void	Resize(uint32_t NewIndexSize);

        // Functions used to search
        uint32_t			First(uint32_t Key) const;
        uint32_t			Next(uint32_t Index) const;
        bool			IsValid(uint32_t Index) const;

        void			Add(uint32_t Key, uint32_t Index);
        //void			Add_Concurrent(uint32_t Key, uint32_t Index);
        void			Remove(uint32_t Key, uint32_t Index);

        // Average # of compares per search
        float	AverageSearch() const;

    protected:
        // Avoids allocating hash until first add
        static uint32_t	EmptyHash[1];

        uint32_t			HashSize;
        uint32_t			HashMask;
        uint32_t			IndexSize;

        uint32_t* Hash;
        uint32_t* NextIndex;
    };
    inline void FHashTable::Remove(uint32_t Key, uint32_t Index)
    {
        if (Index >= IndexSize)
        {
            return;
        }

        Key &= HashMask;

        if (Hash[Key] == Index)
        {
            // Head of chain
            Hash[Key] = NextIndex[Index];
        }
        else
        {
            for (uint32_t i = Hash[Key]; IsValid(i); i = NextIndex[i])
            {
                if (NextIndex[i] == Index)
                {
                    // Next = Next->Next
                    NextIndex[i] = NextIndex[Index];
                    break;
                }
            }
        }
    }
}