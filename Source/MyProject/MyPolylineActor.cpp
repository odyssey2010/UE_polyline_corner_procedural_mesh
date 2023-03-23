// MIT License
// Copyright (c) 2023 devx.3dcodekits.com
// All rights reserved.
// 
// Permission is hereby granted, free of charge, to any person
// obtaining a copy of this softwareand associated documentation
// files(the "Software"), to deal in the Software without
// restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and /or sell
// copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following
// conditions :
//
// The above copyright noticeand this permission notice shall be
// included in all copies or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

#include "MyPolylineActor.h"
#include "Engine/World.h"
#include "Camera/PlayerCameraManager.h"

#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "ProceduralMeshComponent.h"

// Sets default values
AMyPolylineActor::AMyPolylineActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	ProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralMeshComponent"));
	ProceduralMesh->SetupAttachment(RootComponent);

	static FString Path = TEXT("/Game/M_PolylineMaterial");
	static ConstructorHelpers::FObjectFinder<UMaterial> MaterialLoader(*Path);

	if (MaterialLoader.Succeeded())
	{
		Material = MaterialLoader.Object;

		ProceduralMesh->SetMaterial(0, Material);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to load material at path: %s"), *Path);
	}
}

void AMyPolylineActor::BeginPlay()
{
	Super::BeginPlay();
	
	BuildMesh();
}

void AMyPolylineActor::PostLoad()
{
	Super::PostLoad();

	if (0)
	{
		Points =
		{
			{ 100, 0, 0 },
			{ 0, -100, 0 },
			//{ 0, 100, 0 },
			{ -100, 0, 0 },
		};

		Thickness = 100;

		//CutCorner = ArrowCut;
		//BuildPolyline();

		CutCorner = FlatCut;
		BuildPolylineFlatCut();
		return;
	}

	BuildMesh();
}

#define DEBUGMESSAGE(x, ...) if(GEngine){GEngine->AddOnScreenDebugMessage(-1, 60.0f, FColor::Red, FString::Printf(TEXT(x), __VA_ARGS__));}

void AMyPolylineActor::BuildPolylineTightCut()
{
	TArray<FVector> Vertices;
	TArray<FLinearColor> Colors;
	TArray<int32> Triangles;

	FVector Point, PrevPoint, NextPoint;
	FVector PrevDir, NextDir, PrevOutDir, NextOutDir;
	FVector OutDir, OutDirLeft, UpDir = FVector::ZAxisVector; // Outward, Upward Direction
	FVector V0, V1, V2;
	int32 I0, I1, I2, PrevI0 = 0, PrevI1 = 0;

	// Triangle points of corner
	FVector InnerPoint, LeftPoint, RightPoint;

	const float HalfThickness = Thickness * 0.5f;
	bool IsCorner = false;
	float CornerSign = 0.0f;

	for (int32 i = 0, NumPoly = Points.Num(); i < NumPoly; ++i)
	{
		Point = Points[i];
		NextPoint = Points[(i < (NumPoly - 1)) ? (i + 1) : 0];

		IsCorner = (0 < i && i < (NumPoly - 1));

		if (IsCorner == false)
		{
			if (i == 0)
			{
				PrevDir = (NextPoint - Point).GetSafeNormal();
				OutDir = UpDir.Cross(PrevDir);
			}
			else if (i == (NumPoly - 1))
			{
				PrevDir = (Point - PrevPoint).GetSafeNormal();
				OutDir = UpDir.Cross(PrevDir);
			}

			V0 = Point + OutDir * HalfThickness;
			V1 = Point - OutDir * HalfThickness;

			I0 = Vertices.Add(V0);
			I1 = Vertices.Add(V1);

			// Add rectangle of start and end segment
			Triangles.Append({ I0, I1, PrevI0, PrevI0, I1, PrevI1 });

			Colors.Append({ Color, Color });

			PrevI0 = I0;
			PrevI1 = I1;
		}
		else
		{
			PrevDir = (Point - PrevPoint).GetSafeNormal();
			PrevOutDir = UpDir.Cross(PrevDir);

			NextDir = (NextPoint - Point).GetSafeNormal();
			NextOutDir = UpDir.Cross(NextDir);

			OutDir = (PrevOutDir + NextOutDir).GetSafeNormal();

			// If the value of cos(еш) is positive, the angle between the vectors is acute.
			// If the value of cos(еш) is negative, the angle between the vectors is obtuse.
			CornerSign = (NextDir.Cross(PrevDir).Z > 0.0f) ? 1.0f : -1.0f;

			// Calculate triangle points of corner
			float CosTheta = NextDir.Dot(-PrevDir);
			float Angle = FMath::Acos(CosTheta);

			float SinValue = FMath::Sin(Angle * 0.5f);
			float OutDirScale = FMath::IsNearlyZero(SinValue, 0.01f) ? 1000.0f : (1.0f / SinValue);

			// Calculate triangle points of corner at tight cut.
			FVector DirLeft = UpDir.Cross(NextDir);
			FVector DirRight = UpDir.Cross(PrevDir);

			InnerPoint = Point - OutDir * (OutDirScale * HalfThickness * CornerSign);
			LeftPoint = InnerPoint + DirLeft * (Thickness * CornerSign);
			RightPoint = InnerPoint + DirRight * (Thickness * CornerSign);

			if (CornerSign > 0.0f)
			{
				V0 = RightPoint;
				V1 = InnerPoint;
				V2 = LeftPoint;
			}
			else
			{
				V0 = InnerPoint;
				V1 = RightPoint;
				V2 = LeftPoint;
			}

			I0 = Vertices.Add(V0);
			I1 = Vertices.Add(V1);
			I2 = Vertices.Add(V2);

			// Add rectangle of start and end segment
			Triangles.Append({ I0, I1, PrevI0, PrevI0, I1, PrevI1 });

			// Add triangle of corner
			Triangles.Append({ I2, I1, I0 });

			Colors.Append({ Color, Color, Color });

			if (CornerSign > 0.0f)
			{
				PrevI0 = I2;
				PrevI1 = I1;
			}
			else
			{
				PrevI0 = I0;
				PrevI1 = I2;
			}
		}

		PrevPoint = Point;
	}

	ProceduralMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, {}, {}, Colors, {}, false);
}


void AMyPolylineActor::BuildPolylineFlatCut()
{
	TArray<FVector> Vertices;
	TArray<FLinearColor> Colors;
	TArray<int32> Triangles;

	FVector Point, PrevPoint, NextPoint;
	FVector PrevDir, NextDir, PrevOutDir, NextOutDir;
	FVector OutDir, OutDirLeft, UpDir = FVector::ZAxisVector; // Outward, Upward Direction
	FVector V0, V1, V2;
	int32 I0, I1, I2, PrevI0 = 0, PrevI1 = 0;

	// Triangle points of corner
	FVector InnerPoint, LeftPoint, RightPoint;

	const float HalfThickness = Thickness * 0.5f;
	bool IsCorner = false;
	float CornerSign = 0.0f;

	for (int32 i = 0, NumPoly = Points.Num(); i < NumPoly; ++i)
	{
		Point = Points[i];
		NextPoint = Points[(i < (NumPoly - 1)) ? (i + 1) : 0];

		IsCorner = (0 < i && i < (NumPoly - 1));

		if (IsCorner == false)
		{
			if (i == 0)
			{
				PrevDir = (NextPoint - Point).GetSafeNormal();
				OutDir = UpDir.Cross(PrevDir);
			}
			else if (i == (NumPoly - 1))
			{
				PrevDir = (Point - PrevPoint).GetSafeNormal();
				OutDir = UpDir.Cross(PrevDir);
			}

			V0 = Point + OutDir * HalfThickness;
			V1 = Point - OutDir * HalfThickness;

			I0 = Vertices.Add(V0);
			I1 = Vertices.Add(V1);

			// Add rectangle of start and end segment
			Triangles.Append({ I0, I1, PrevI0, PrevI0, I1, PrevI1 });

			Colors.Append({ Color, Color });

			PrevI0 = I0;
			PrevI1 = I1;
		}
		else
		{
			PrevDir = (Point - PrevPoint).GetSafeNormal();
			PrevOutDir = UpDir.Cross(PrevDir);

			NextDir = (NextPoint - Point).GetSafeNormal();
			NextOutDir = UpDir.Cross(NextDir);

			OutDir = (PrevOutDir + NextOutDir).GetSafeNormal();

			// If the value of cos(еш) is positive, the angle between the vectors is acute.
			// If the value of cos(еш) is negative, the angle between the vectors is obtuse.
			CornerSign = (NextDir.Cross(PrevDir).Z > 0.0f) ? 1.0f : -1.0f;

			// Calculate triangle points of corner with tickness.
			float CosTheta = NextDir.Dot(-PrevDir);
			float Angle = FMath::Acos(CosTheta);

			float SinValue = FMath::Sin(Angle * 0.5f);
			float OutDirScale = FMath::IsNearlyZero(SinValue, 0.01f) ? 1000.0f : (1.0f / SinValue);

			float TanAngle = UE_PI - Angle;
			float TanScale = FMath::Tan(TanAngle * 0.25f);

			OutDirLeft = OutDir.Cross(UpDir);
			InnerPoint = Point - OutDir * (OutDirScale * HalfThickness * CornerSign);
			LeftPoint  = Point + OutDir * (HalfThickness * CornerSign) + OutDirLeft * (HalfThickness * TanScale);
			RightPoint = Point + OutDir * (HalfThickness * CornerSign) - OutDirLeft * (HalfThickness * TanScale);

			if (CornerSign > 0.0f)
			{
				V0 = RightPoint;
				V1 = InnerPoint;
				V2 = LeftPoint;
			}
			else
			{
				V0 = InnerPoint;
				V1 = RightPoint;
				V2 = LeftPoint;
			}

			I0 = Vertices.Add(V0);
			I1 = Vertices.Add(V1);					
			I2 = Vertices.Add(V2);
				
			// Add rectangle of start and end segment
			Triangles.Append({ I0, I1, PrevI0, PrevI0, I1, PrevI1 });

			// Add triangle of corner
			Triangles.Append({ I2, I1, I0 });

			Colors.Append({ Color, Color, Color });
			
			if (CornerSign > 0.0f)
			{
				PrevI0 = I2;
				PrevI1 = I1;
			}
			else
			{
				PrevI0 = I0;
				PrevI1 = I2;
			}
		}

		PrevPoint = Point;
	}

	ProceduralMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, {}, {}, Colors, {}, false);
}

// Called every frame
void AMyPolylineActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	float Roll = RotationSpeed.X * DeltaTime;
	float Pitch = RotationSpeed.Y * DeltaTime;
	float Yaw = RotationSpeed.Z * DeltaTime;

	FQuat Curr = GetActorRotation().Quaternion();
	FQuat Add = FRotator(Pitch, Yaw, Roll).Quaternion();
	FQuat Rotation = Curr * Add;

	SetActorRotation(Rotation);
}

void AMyPolylineActor::BuildMesh()
{
	if (CutCorner == CornerTypeEnum::TightCut)
	{
		BuildPolylineTightCut();
	}
	else if (CutCorner == CornerTypeEnum::FlatCut)
	{
		BuildPolylineFlatCut();
	}
}

void AMyPolylineActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property) 
	{
		BuildMesh();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
