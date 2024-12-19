#include "Hooking.Patterns.h"
#include "injector/injector.hpp"
#include "rage/LightSource.h"
#include "rage/Weather.h"

#include <Windows.h>
#include <fstream>
#include <cstdint>

float gVolumeIntensity = 1.0f;
float gCfgVolumetricIntensityMultiplier = 1.0f;
bool gCfgAlwaysEnabled = false;

void (__cdecl *CRenderPhaseDeferredLighting_LightsToScreen__BuildRenderListO)() = nullptr;
void (__cdecl *CopyLightO)() = nullptr;

void DisplayUnsupportedError();
void ReadConfig();
void Update();
void OnAfterCopyLight(rage::CLightSource*);

bool HasSnow(CWeather::eWeatherType type);

void __declspec(naked) CRenderPhaseDeferredLighting_LightsToScreen__BuildRenderListH()
{
	_asm
	{
		push ecx
		call Update

		pop ecx
		call CRenderPhaseDeferredLighting_LightsToScreen__BuildRenderListO

		ret
	}
}

void __declspec(naked) CopyLightH()
{
	_asm
	{
		push [esp+0x4]
		call CopyLightO

		push eax
		call OnAfterCopyLight
		pop eax

		ret 0x4
	}
}

BOOL WINAPI DllMain(HINSTANCE, DWORD fdwReason, LPVOID)
{
	if(fdwReason == DLL_PROCESS_ATTACH)
	{
		ReadConfig();

		if(!CWeather::Init())
		{
			DisplayUnsupportedError();
			return false;
		}

		auto pattern = hook::pattern("D9 1C 24 F3 0F 10 05 ? ? ? ? 8D 4C 24 7C F3 0F 11 54 24 ?");
		if(pattern.empty())
		{
			DisplayUnsupportedError();
			return false;
		}
		//movss xmm0, dword ptr [esi + eax * 1 + 0x4C] or xmm0 = Lights::mRenderLights[i].field_C
		uint8_t buffer[] = {0x44, 0x06, 0xC, 0x90, 0x90};
		injector::WriteMemoryRaw(pattern.get_first(6), buffer, 5, true);

		pattern = hook::pattern("C7 06 ? ? ? ? C7 86 ? ? ? ? ? ? ? ? C6 46 1C 01 8B C6");
		if(pattern.empty())
		{
			DisplayUnsupportedError();
			return false;
		}
		uintptr_t* vft = *(uintptr_t**)pattern.get_first(2);
		CRenderPhaseDeferredLighting_LightsToScreen__BuildRenderListO = (void(__cdecl*)())vft[8];
		injector::WriteMemory(&vft[8], CRenderPhaseDeferredLighting_LightsToScreen__BuildRenderListH);

		pattern = hook::pattern("8B CE C1 E1 07 03 0D ? ? ? ? E8 ? ? ? ?");
		if(pattern.empty())
		{
			DisplayUnsupportedError();
			return false;
		}
		CopyLightO = (void(__cdecl*)())injector::GetBranchDestination(pattern.get_first(11)).get();
		injector::MakeCALL(pattern.get_first(11), CopyLightH);

		pattern = hook::pattern("8B 54 24 08 8B C8 C1 E1 07 03 0D ? ? ? ? 52 E8 ? ? ? ?");
		if(pattern.empty())
		{
			DisplayUnsupportedError();
			return false;
		}
		injector::MakeCALL(pattern.get_first(16), CopyLightH);
	}

	return true;
}

void DisplayUnsupportedError()
{
	MessageBox(0, L"Unsupported Game Version", L"Snow.asi", MB_ICONERROR | MB_OK);
}

void ReadConfig()
{
	std::ifstream cfgFile("VolumetricLights.cfg");
	if(cfgFile.is_open())
	{
		std::string currLine;
		while(std::getline(cfgFile, currLine))
		{
			size_t index;
			index = currLine.rfind("VolumetricIntensityMultiplier");
			if(index != std::string::npos)
			{
				index += sizeof("VolumetricIntensityMultiplier");

				while(index < currLine.size())
				{
					if(currLine[index] != ' ' && currLine[index] != '=')
					{
						break;
					}

					index++;
				}

				gCfgVolumetricIntensityMultiplier = std::stof(currLine.substr(index));
			}

			if(currLine.rfind("AlwaysEnabled") != std::string::npos)
			{
				if(currLine.rfind("true") != std::string::npos)
				{
					gCfgAlwaysEnabled = true;
				}
			}
		}
	}
}

void Update()
{
	if(gCfgAlwaysEnabled)
	{
		gVolumeIntensity = 4.0f * gCfgVolumetricIntensityMultiplier;
		return;
	}

	gVolumeIntensity = 0.0f;

	static CWeather::eWeatherType currWeather;

	CWeather::eWeatherType prevWeather = currWeather;
	currWeather = CWeather::GetCurrentWeather();
	CWeather::eWeatherType nextWeather = CWeather::GetNextWeather();

	if(HasSnow(prevWeather) || HasSnow(currWeather) || HasSnow(nextWeather))
	{
		float threshold;
		if(!HasSnow(currWeather) && HasSnow(nextWeather))
			threshold = CWeather::GetNextWeatherPercentage();
		else if(HasSnow(currWeather) && !HasSnow(nextWeather))
			threshold = 0.9999f - CWeather::GetNextWeatherPercentage();
		else if(!HasSnow(currWeather) && !HasSnow(nextWeather))
			threshold = 0.0f;
		else
			threshold = 0.9999f;

		gVolumeIntensity = threshold * 4.0f * gCfgVolumetricIntensityMultiplier;
	}
}

void OnAfterCopyLight(rage::CLightSource *light)
{
	//CLightSource doesnt have a member to control the volume intensity so
	//i abuse type casting to use field_C for it as im p sure its just padding anyway

	if(light->mIntensityTextureMaskHash == 0xDEAD)
	{
		light->mFlags &= ~8;
	}
	else if(light->mFlags & 8 /*volumetric*/)
	{
		*(float*)&light->field_C = 1.0f;
	}
	else if(light->mType == rage::LT_SPOT && !(light->mFlags & 0x300)/*vehicles and traffic lights*/)
	{
		if(gVolumeIntensity > 0.0f && light->mRadius < 50.0f)
		{
			light->mVolumeSize = 1.0f;
			light->mVolumeScale = 0.5f;
			light->mFlags |= 8;
			*(float*)&light->field_C = gVolumeIntensity;
		}
	}
}

bool HasSnow(CWeather::eWeatherType type)
{
	switch(type)
	{
		case CWeather::RAIN:
		case CWeather::DRIZZLE:
		case CWeather::LIGHTNING:
		case CWeather::FOGGY:
			return true;
		default:
			return false;
	}
}