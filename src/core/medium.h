#pragma once
#include "common.h"
#include "krrmath/math.h"
#include "device/taggedptr.h"
#include "render/spectrum.h"

NAMESPACE_BEGIN(krr)

class Ray;

class HGPhaseFunction;
class PhaseFunctionSample;
class HomogeneousMedium;
class MediumProperties;
class MajorantIterator;
template <typename DataType> class NanoVDBMedium;

class PhaseFunction : public TaggedPointer<HGPhaseFunction> {
public:
	using TaggedPointer::TaggedPointer;

	KRR_HOST_DEVICE PhaseFunctionSample sample(const Vector3f &wo, const Vector2f &u) const;

	KRR_HOST_DEVICE float pdf(const Vector3f &wo, const Vector3f &wi) const;

	KRR_HOST_DEVICE float p(const Vector3f &wo, const Vector3f &wi) const;
};

struct MediumProperties {
	Spectrum sigma_a, sigma_s;
	PhaseFunction phase;
	Spectrum Le;
};

class Medium : public TaggedPointer<HomogeneousMedium, NanoVDBMedium<float>, NanoVDBMedium<Array3f>> {
public:
	using TaggedPointer::TaggedPointer;

	KRR_CALLABLE bool isEmissive() const;

	KRR_CALLABLE MediumProperties samplePoint(Vector3f p, const SampledWavelengths &lambda) const;

	KRR_CALLABLE MajorantIterator sampleRay(const Ray &ray, float tMax,
											const SampledWavelengths &lambda) const;
};

class MediumInterface {
public:
	KRR_CALLABLE MediumInterface() = default;
	KRR_CALLABLE MediumInterface(Medium m) : inside(m), outside(m) {}
	KRR_CALLABLE MediumInterface(Medium mi, Medium mo) : inside(mi), outside(mo) {}

	KRR_CALLABLE bool isTransition() const { return inside != outside; }

	Medium inside, outside;
};

NAMESPACE_END(krr)