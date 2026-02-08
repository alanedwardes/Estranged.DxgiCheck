#include "EstDxgiCheckModule.h"
#include "Modules/ModuleManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/App.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformMisc.h"

#include "Windows/AllowWindowsPlatformTypes.h"
#include <d3d12.h>
#include <dxgi1_6.h>
#include "Windows/HideWindowsPlatformTypes.h"

IMPLEMENT_MODULE(FEstDxgiCheckModule, EstDxgiCheck)

enum class EHardwareFeature : uint32
{
	None = 0,
	DX12 = 1 << 0,
	Atomic64 = 1 << 1,
	WaveOps = 1 << 2,
	SM66 = 1 << 3,
	RayTracing = 1 << 4,
	MeshShaders = 1 << 5,
	VRS = 1 << 6,
	SimulatedFailure = 1 << 7,
	Max = 1 << 8,
};

ENUM_CLASS_FLAGS(EHardwareFeature);

static EHardwareFeature DetectHardwareFeatures(ID3D12Device* Device)
{
	EHardwareFeature Features = EHardwareFeature::DX12;

	D3D12_FEATURE_DATA_D3D12_OPTIONS9 Caps9{};
	if (SUCCEEDED(Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS9, &Caps9, sizeof(Caps9))))
	{
		if (Caps9.AtomicInt64OnTypedResourceSupported)
		{
			Features |= EHardwareFeature::Atomic64;
		}
	}

	D3D12_FEATURE_DATA_D3D12_OPTIONS1 Caps1{};
	if (SUCCEEDED(Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS1, &Caps1, sizeof(Caps1))))
	{
		if (Caps1.WaveOps)
		{
			Features |= EHardwareFeature::WaveOps;
		}
	}

	D3D12_FEATURE_DATA_SHADER_MODEL SMCaps = { D3D_SHADER_MODEL_6_6 };
	if (SUCCEEDED(Device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &SMCaps, sizeof(SMCaps))))
	{
		if (SMCaps.HighestShaderModel >= D3D_SHADER_MODEL_6_6)
		{
			Features |= EHardwareFeature::SM66;
		}
	}

	D3D12_FEATURE_DATA_D3D12_OPTIONS5 Caps5{};
	if (SUCCEEDED(Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &Caps5, sizeof(Caps5))))
	{
		if (Caps5.RaytracingTier >= D3D12_RAYTRACING_TIER_1_1)
		{
			Features |= EHardwareFeature::RayTracing;
		}
	}

	D3D12_FEATURE_DATA_D3D12_OPTIONS7 Caps7{};
	if (SUCCEEDED(Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &Caps7, sizeof(Caps7))))
	{
		if (Caps7.MeshShaderTier >= D3D12_MESH_SHADER_TIER_1)
		{
			Features |= EHardwareFeature::MeshShaders;
		}
	}

	D3D12_FEATURE_DATA_D3D12_OPTIONS6 Caps6{};
	if (SUCCEEDED(Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS6, &Caps6, sizeof(Caps6))))
	{
		if (Caps6.VariableShadingRateTier >= D3D12_VARIABLE_SHADING_RATE_TIER_2)
		{
			Features |= EHardwareFeature::VRS;
		}
	}

	return Features;
}

static EHardwareFeature GetRequiredFeatures()
{
	EHardwareFeature Required = EHardwareFeature::DX12;

	if (!GConfig)
	{
		return Required;
	}

	const TCHAR* RendererSection = TEXT("/Script/Engine.RendererSettings");

	int32 NaniteEnabled = 0;
	GConfig->GetInt(RendererSection, TEXT("r.Nanite"), NaniteEnabled, GEngineIni);
	if (NaniteEnabled > 0)
	{
		Required |= EHardwareFeature::Atomic64 | EHardwareFeature::WaveOps | EHardwareFeature::SM66;
	}

	int32 VSMEnabled = 0;
	GConfig->GetInt(RendererSection, TEXT("r.Shadow.Virtual.Enable"), VSMEnabled, GEngineIni);
	if (VSMEnabled > 0)
	{
		Required |= EHardwareFeature::SM66;
	}

	int32 RayTracingEnabled = 0;
	GConfig->GetInt(RendererSection, TEXT("r.RayTracing"), RayTracingEnabled, GEngineIni);
	if (RayTracingEnabled > 0)
	{
		Required |= EHardwareFeature::RayTracing;
	}

#if !UE_BUILD_SHIPPING
	const TCHAR* PluginSection = TEXT("/Script/EstDxgiStats.EstDxgiCheck");
	bool SimulateFailure = false;
	GConfig->GetBool(PluginSection, TEXT("SimulateFailure"), SimulateFailure, GEngineIni);
	if (SimulateFailure)
	{
		Required |= EHardwareFeature::SimulatedFailure;
	}
#endif

	return Required;
}

static const TCHAR* GetFeatureName(EHardwareFeature Feature)
{
	switch (Feature)
	{
		case EHardwareFeature::DX12: return TEXT("DirectX 12");
		case EHardwareFeature::Atomic64: return TEXT("DirectX 12 64-bit Atomics");
		case EHardwareFeature::WaveOps: return TEXT("DirectX 12 Wave Operations");
		case EHardwareFeature::SM66: return TEXT("Shader Model 6.6");
		case EHardwareFeature::RayTracing: return TEXT("DirectX 12 Ray Tracing (Tier 1.1)");
		case EHardwareFeature::MeshShaders: return TEXT("DirectX 12 Mesh Shaders");
		case EHardwareFeature::VRS: return TEXT("DirectX 12 Variable Rate Shading (Tier 2)");
		case EHardwareFeature::SimulatedFailure: return TEXT("Simulated Failure (Development)");
		default: return TEXT("Unknown");
	}
}

static const TCHAR* GetFeatureId(EHardwareFeature Feature)
{
	switch (Feature)
	{
		case EHardwareFeature::DX12: return TEXT("dx12");
		case EHardwareFeature::Atomic64: return TEXT("atomic64");
		case EHardwareFeature::WaveOps: return TEXT("waveops");
		case EHardwareFeature::SM66: return TEXT("sm66");
		case EHardwareFeature::RayTracing: return TEXT("raytracing");
		case EHardwareFeature::MeshShaders: return TEXT("meshshaders");
		case EHardwareFeature::VRS: return TEXT("vrs");
		case EHardwareFeature::SimulatedFailure: return TEXT("simulated");
		default: return TEXT("unknown");
	}
}

static void ShowErrorAndExit(EHardwareFeature Detected, EHardwareFeature Required)
{
	FString ProblemMessage = TEXT("Your graphics card does not support the required features for this game.");
	FString QuestionMessage = TEXT("Would you like to learn more?");
	FString ErrorTitle = TEXT("Incompatible Hardware");
	FString HelpURL = TEXT("https://example.com/");

	if (GConfig)
	{
		const TCHAR* Section = TEXT("/Script/EstDxgiStats.EstDxgiCheck");
		GConfig->GetString(Section, TEXT("ProblemMessage"), ProblemMessage, GEngineIni);
		GConfig->GetString(Section, TEXT("QuestionMessage"), QuestionMessage, GEngineIni);
		GConfig->GetString(Section, TEXT("ErrorTitle"), ErrorTitle, GEngineIni);
		GConfig->GetString(Section, TEXT("HelpURL"), HelpURL, GEngineIni);
	}

	FString MissingList;
	FString QueryParams;

	for (uint32 Bit = 1; Bit < static_cast<uint32>(EHardwareFeature::Max); Bit <<= 1)
	{
		EHardwareFeature Feature = static_cast<EHardwareFeature>(Bit);
		if (EnumHasAllFlags(Required, Feature) && !EnumHasAllFlags(Detected, Feature))
		{
			MissingList += FString::Printf(TEXT("\n\u2022 %s"), GetFeatureName(Feature));
			QueryParams += QueryParams.IsEmpty() ? TEXT("?") : TEXT("&");
			QueryParams += FString::Printf(TEXT("missing=%s"), GetFeatureId(Feature));
		}
	}

	FString FinalMessage = FString::Printf(TEXT("%s\n\nMissing Features:%s\n\n%s"), *ProblemMessage, *MissingList, *QuestionMessage);
	EAppReturnType::Type Result = FPlatformMisc::MessageBoxExt(EAppMsgType::YesNo, *FinalMessage, *ErrorTitle);
	
	if (Result == EAppReturnType::Yes && !HelpURL.IsEmpty())
	{
		FString FinalURL = HelpURL + QueryParams;
		FPlatformProcess::LaunchURL(*FinalURL, nullptr, nullptr);
	}

	FPlatformMisc::RequestExit(true, TEXT("EstDxgiCheck.RequiredFeaturesMissing"));
}

void FEstDxgiCheckModule::StartupModule()
{
	EHardwareFeature RequiredFeatures = GetRequiredFeatures();
	if (RequiredFeatures == EHardwareFeature::None)
	{
		return;
	}

	ID3D12Device* Device = nullptr;
	HRESULT Hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&Device));
	
	EHardwareFeature DetectedFeatures = EHardwareFeature::None;
	if (SUCCEEDED(Hr) && Device != nullptr)
	{
		DetectedFeatures = DetectHardwareFeatures(Device);
		Device->Release();
	}

	if (!EnumHasAllFlags(DetectedFeatures, RequiredFeatures))
	{
		ShowErrorAndExit(DetectedFeatures, RequiredFeatures);
	}
}

void FEstDxgiCheckModule::ShutdownModule()
{
}
