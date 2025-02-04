#pragma once
#include "common.h"
#include "raytracing.h"
#include "render/shared.h"
#include "render/spectrum.h"
#include "render/materials/bxdf.h"

NAMESPACE_BEGIN(krr)
using namespace rt;

/* Remember to copy these definitions to workitem.soa whenever changing them. */

struct PixelState {
	Spectrum L;
	RGB pixel;
	PCGSampler sampler;
	CameraSample cameraSample;
	SampledWavelengths lambda;
};

struct RayWorkItem {
	Ray ray;
	LightSampleContext ctx;
	Spectrum thp;
	Spectrum pu, pl;
	BSDFType bsdfType;
	uint depth;
	uint pixelId;
};

struct MissRayWorkItem {
	Ray ray;
	LightSampleContext ctx;
	Spectrum thp;
	Spectrum pu, pl;
	BSDFType bsdfType;
	uint depth;
	uint pixelId;
};

struct HitLightWorkItem {
	Light light;
	LightSampleContext ctx;
	Vector3f p;
	Vector3f wo;
	Vector3f n;
	Vector2f uv;
	Spectrum thp;
	Spectrum pu, pl;
	BSDFType bsdfType;
	uint depth;
	uint pixelId;
};

struct ShadowRayWorkItem {
	Ray ray;
	float tMax;
	Spectrum Ld;
	Spectrum pu, pl;
	uint pixelId;
};

struct ScatterRayWorkItem {
	Spectrum thp;
	Spectrum pu;
	SurfaceInteraction intr;
	MediumInterface mediumInterface;
	uint depth;
	uint pixelId;
};

struct MediumSampleWorkItem {
	Ray ray;
	LightSampleContext ctx;
	Spectrum thp;
	Spectrum pu, pl;
	float tMax;
	SurfaceInteraction intr;	// has hit a surface as well...
	BSDFType bsdfType;
	uint depth;
	uint pixelId;
};

struct MediumScatterWorkItem {
	Vector3f p;
	Spectrum thp;
	Spectrum pu;
	Vector3f wo;
	float time;
	Medium medium;
	PhaseFunction phase;
	uint depth;
	uint pixelId;
};

#pragma warning (push, 0)
#pragma warning (disable: ALL_CODE_ANALYSIS_WARNINGS)
#include "render/wavefront/workitem_soa.h"
#pragma warning (pop)

NAMESPACE_END(krr)