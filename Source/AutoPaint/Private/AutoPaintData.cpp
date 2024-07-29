// Fill out your copyright notice in the Description page of Project Settings.


#include "AutoPaintData.h"

#if WITH_EDITOR
void UAutoPaintData::PreEditChange(FProperty* PropertyAboutToChange)
{
	UObject::PreEditChange(PropertyAboutToChange);
}

void UAutoPaintData::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UObject::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void UAutoPaintData::AssignStaticMesh(const FAssetData& InStaticMeshAssetData)
{
	ReferencedStaticMesh = Cast<UStaticMesh>(InStaticMeshAssetData.GetAsset());
	MarkPackageDirty();
}