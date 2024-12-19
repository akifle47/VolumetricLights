#pragma once
#include "Vector.h"
#include <cstdint>

namespace rage
{
	enum eLightType
	{
		LT_POINT = 0x0,
		LT_DIR = 0x1,
		LT_SPOT = 0x2,
		LT_AO = 0x3,
		LT_CLAMPED = 0x4,
	};

	class CLightSource
	{
	public:
		Vector3 mDirection;
		float field_C;
		Vector3 mTangent;
		float field_1C;
		Vector3 mPosition;
		float field_2C;
		Vector4 mColor;
		float mIntensity;
		eLightType mType;
		uint32_t mFlags;
		int32_t mTxdId;
		int32_t mIntensityTextureMaskHash;
		float mRadius;
		float mInnerConeAngle;
		float mOuterConeAngle;
		int32_t field_60;
		int32_t field_64;
		int32_t field_68;
		int32_t field_6C;
		float mVolumeSize;
		float mVolumeScale;
		int8_t field_78[7];
		char field_7F;
	};

	static_assert(sizeof(CLightSource) == 0x80);
}