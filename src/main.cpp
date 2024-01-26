#define _CRT_SECURE_NO_WARNINGS

#include <Windows.h>
#include <fstream>
#include <string>
#include <cstdint>

#include "Utils.h"
#include "rage/LightSource.h"
#include "rage/Weather.h"

float gVolumeIntensity = 1.0f;
float gCfgVolumetricIntensityMultiplier = 1.0f;

void (__cdecl *CRenderPhaseDeferredLighting_LightsToScreen__BuildRenderListO)() = nullptr;
void (__cdecl *CopyLightO)() = nullptr;

void Init();
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
		if(*(uint32_t *)Utils::ReadMemory(0x208C34) != 0x404B100F)
		{
			MessageBox(0, L"Only version 1.0.8.0 is supported", L"Snow.asi", MB_ICONERROR | MB_OK);
			return false;
		}
		
		CRenderPhaseDeferredLighting_LightsToScreen__BuildRenderListO = *(void(__cdecl **)())Utils::ReadMemory(0x9E4874);
		Utils::WriteMemory(0x9E4874, CRenderPhaseDeferredLighting_LightsToScreen__BuildRenderListH);

		CopyLightO = (void(__cdecl *)())Utils::ReadMemory(0x62A7F0);
		Utils::WriteCall(0x62DAB7, CopyLightH);
		Utils::WriteCall(0x62DA71, CopyLightH);
	}

	return true;
}

void Init()
{
	static bool initialized = false;
	if(initialized)
		return;

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
		}
	}

	//movss xmm0, dword ptr [esi + eax * 1 + 0x4C] or xmm0 = Lights::mRenderLights[i].mTxdId
	uint8_t buffer[] = {0x44, 0x06, 0x4C, 0x90, 0x90};
	Utils::WriteMemory(0x630B00, buffer, 5);
}

void Update()
{
	Init();

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
	//i abuse type casting to use mTxdId for it as im p sure its only used for headlights anyway

	if(light->mFlags & 8 /*volumetric*/)
	{
		*(float*)&light->mTxdId = 1.0f;
	}
	else if(light->mType == rage::LT_SPOT && !(light->mFlags & 0x300)/*vehicles and traffic lights*/)
	{
		if(gVolumeIntensity > 0.0f && light->mRadius < 50.0f)
		{
			light->mVolumeSize = 1.0f;
			light->mVolumeScale = 0.5f;
			light->mFlags |= 8;
			*(float*)&light->mTxdId = gVolumeIntensity;
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