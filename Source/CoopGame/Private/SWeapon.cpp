// Fill out your copyright notice in the Description page of Project Settings.


#include "SWeapon.h"

#include "CoopGame/CoopGame.h"
#include "Editor/EditorEngine.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Particles/ParticleSystemComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

static int32 DebugWeaponDrawing = 0;
FAutoConsoleVariableRef CVARDebugWeaponDrawing(
	TEXT("COOP.DebugWeapons"),
	DebugWeaponDrawing,
	TEXT("Draw Debug Lines for Weapons"),
	ECVF_Cheat);

// Sets default values
ASWeapon::ASWeapon()
{
	MeshComp = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("MeshComp"));
	RootComponent = MeshComp;

	MuzzleSocketName = "MuzzleSocket";
	TracerTargetName = "BeamEnd";

	BaseDamage = 20.0f;

	//Bullets per minute
	RateOfFire = 600;

	SetReplicates(true);
}

void ASWeapon::BeginPlay()
{
	Super::BeginPlay();

	TimeBetweenShots = 60/RateOfFire;
}

void ASWeapon::PlayFireEffects(FVector TraceEnd)
{
	//Muzzle Effect VFX
	if(MuzzleEffect)
	{
		UGameplayStatics::SpawnEmitterAttached(MuzzleEffect, MeshComp, MuzzleSocketName);
	}

	//TracerEffect VFX
	if (TracerEffect)
	{
		FVector MuzzleLocation = MeshComp->GetSocketLocation(MuzzleSocketName);
		UParticleSystemComponent* TracerComp = UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), TracerEffect, MuzzleLocation);
		if(TracerComp)
		{
			//Set how long the trail Effect of the bullet when impacts with the enemy ot anything 
			TracerComp->SetVectorParameter(TracerTargetName, TraceEnd);
		}
	}

	//Camera Shake
	APawn* MyOwner = Cast<APawn>(GetOwner());
	if(MyOwner)
	{
		APlayerController* PC = Cast<APlayerController>(MyOwner->GetController());
		if(PC)
		{
			PC->ClientStartCameraShake(FireCamShake);
		}
	}
}

void ASWeapon::PlayImpactEffects(EPhysicalSurface SurfaceType, FVector ImpactPoint)
{
	//PHYSICAL MATERIAL
	UParticleSystem* SelectedEffect = nullptr;

	switch (SurfaceType)
	{
	case SURFACE_FLESHDEFAULT:
	case SURFACE_FLESHVULNERABLE:
		SelectedEffect = FleshImpactEffect;
		break;
	default:
		SelectedEffect = DefaultImpactEffect;
		break;
	}
	//Impact Effect VFX
	if (SelectedEffect)
	{
		FVector MuzzleLocation = MeshComp->GetSocketLocation(MuzzleSocketName);

		FVector ShotDirection = ImpactPoint - MuzzleLocation;

		ShotDirection.Normalize();
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), SelectedEffect, ImpactPoint, ShotDirection.Rotation());	
	}
}

void ASWeapon::Fire()
{
	if(!HasAuthority())
	{
		ServerFire();
	}
	
	AActor* MyOwner = GetOwner();
	if(MyOwner)
	{
		FVector EyeLocation;
		FRotator EyeRotation;
		MyOwner->GetActorEyesViewPoint(EyeLocation, EyeRotation);

		FVector ShotDirection = EyeRotation.Vector();
		FVector TraceEnd = EyeLocation + (ShotDirection * 10000);
		
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(MyOwner);
		QueryParams.AddIgnoredActor(this);
		/*it will trace against each individual triangle of the mesh that we're hitting.
		 * this is more expensive, but it also gives us the exact result of where we hit something. */
		QueryParams.bTraceComplex = true;

		//PHYSICAL MATERIAL
		//We need to activate this to have Flesh and Default effects spawned.
		QueryParams.bReturnPhysicalMaterial = true;
		
		//TracerEffect VFX
		//Defines how long is the trail effect when impact with anything.
		FVector TracerEndPoint = TraceEnd;

		EPhysicalSurface SurfaceType = SurfaceType_Default;
		
		FHitResult Hit;
		if(GetWorld()->LineTraceSingleByChannel(Hit, EyeLocation, TraceEnd, COLLISION_WEAPON, QueryParams))
		{
			//Blocking hit! Process damage
			AActor* HitActor = Hit.GetActor();
			
			SurfaceType = UPhysicalMaterial::DetermineSurfaceType(Hit.PhysMaterial.Get());

			//HEADSHOT Bonus Damage
			float ActualDamage = BaseDamage;
			if (SurfaceType == SURFACE_FLESHVULNERABLE)
			{
				ActualDamage *= 4.0f; 
			}
			
			UGameplayStatics::ApplyPointDamage(HitActor, ActualDamage, ShotDirection, Hit, MyOwner->GetInstigatorController(), this, DamageType);

			PlayImpactEffects(SurfaceType, Hit.ImpactPoint);
			
			//TracerEffect VFX
			//Defines the end point for the trail of the bullet
			//Defines how long is the trail effect when impact with the enemy
			TracerEndPoint = Hit.ImpactPoint;
			
		}

		if (DebugWeaponDrawing > 0)
		{
			DrawDebugLine(GetWorld(), EyeLocation, TraceEnd, FColor::Red, false,1.0f, 0, 1.0f);	
		}

		PlayFireEffects(TracerEndPoint);

		if(HasAuthority())
		{
			HitScanTrace.TraceTo = TracerEndPoint;
			HitScanTrace.SurfaceType = SurfaceType;
		}

		LastFireTime = GetWorld()->TimeSeconds;
	}
}

void ASWeapon::OnRep_HitScanTrace()
{
	PlayFireEffects(HitScanTrace.TraceTo);

	PlayImpactEffects(HitScanTrace.SurfaceType, HitScanTrace.TraceTo);
}

void ASWeapon::ServerFire_Implementation()
{
	Fire();
}

bool ASWeapon::ServerFire_Validate()
{
	return true;
}

void ASWeapon::StartFire()
{
	float FirstDelay = FMath::Max(LastFireTime + TimeBetweenShots - GetWorld()->TimeSeconds, 0.0f);

	GetWorldTimerManager().SetTimer(TimerHandle_TimeBetweenShots, this, &ASWeapon::Fire, TimeBetweenShots, true, FirstDelay);
}

void ASWeapon::StopFire()
{
	GetWorldTimerManager().ClearTimer(TimerHandle_TimeBetweenShots);
}


void ASWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ASWeapon, HitScanTrace, COND_SkipOwner);
}
