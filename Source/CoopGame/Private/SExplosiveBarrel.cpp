// Fill out your copyright notice in the Description page of Project Settings.


#include "SExplosiveBarrel.h"

#include "Kismet/GameplayStatics.h"

// Sets default values
ASExplosiveBarrel::ASExplosiveBarrel()
{
	HealthComponent = CreateDefaultSubobject<USHealthComponent>(TEXT("HealthComp"));
	HealthComponent->OnHealthChanged.AddDynamic(this, &ASExplosiveBarrel::OnHealthChange);

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshCompt"));
	MeshComp->SetSimulatePhysics(true);
	MeshComp->SetCollisionObjectType(ECC_PhysicsBody);
	RootComponent = MeshComp;

	RadialForceComp = CreateDefaultSubobject<URadialForceComponent>(TEXT("RadialForceComp"));
	RadialForceComp->SetupAttachment(MeshComp);
	RadialForceComp->Radius = 250;
	RadialForceComp->bImpulseVelChange = true;
	RadialForceComp->bAutoActivate = false; //Prevent the component from ticking and only use FireImpulse to activate
	RadialForceComp->bIgnoreOwningActor = true;

	ExplosionIMpulse = 400;

}

void ASExplosiveBarrel::OnHealthChange(USHealthComponent* OwningHealthComp, float Health, float HealthDelta,
	const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	if (bExploded)
	{
		return;
	}

	if(Health <= 0.0f)
	{
		bExploded = true;
		FVector BoostIntensity = FVector::UpVector* ExplosionIMpulse;
		MeshComp->AddImpulse(BoostIntensity, NAME_None, true);

		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ExplosionEffect, GetActorLocation());

		MeshComp->SetMaterial(0, ExplodedMaterial);

		RadialForceComp->FireImpulse();

		//@TODO: Apply radial damage
	}
}
