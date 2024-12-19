#pragma once
#include "Hooking.Patterns.h"

namespace CWeather
{
	enum eWeatherType : uint32_t
	{
		EXTRASUNNY,
		SUNNY,
		SUNNY_WINDY,
		CLOUDY,
		RAIN,
		DRIZZLE,
		FOGGY,
		LIGHTNING
	};

	static eWeatherType* msCurrentWeather = nullptr;
	static eWeatherType* msNextWeather = nullptr;
	static float* msNextWeatherPercentage = nullptr;

	bool Init()
	{
		auto pattern = hook::pattern("8B 0D ? ? ? ? 89 08 C3 8B 54 24 04 A1 ? ? ? ?");
		if(pattern.empty())
			return false;

		msCurrentWeather = *(eWeatherType**)pattern.get_first(2);
		msNextWeather = *(eWeatherType**)pattern.get_first(14);
		msNextWeatherPercentage = *(float**)pattern.get_first(-10);

		return true;
	}

	static eWeatherType GetCurrentWeather()
	{
		return *msCurrentWeather;
	}

	static eWeatherType GetNextWeather()
	{
		return *msNextWeather;
	}

	static float GetNextWeatherPercentage()
	{
		return *msNextWeatherPercentage;
	}
}