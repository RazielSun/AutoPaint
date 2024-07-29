#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FAutoPaintEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

protected:
    TSharedPtr<class FAssetTypeActions_AutoPaintSettings> AssetTypeAction;
    FDelegateHandle ContentBrowserAssetsExtenderDelegateHandle;
};
