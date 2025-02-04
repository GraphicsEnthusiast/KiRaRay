#pragma once
#include "common.h"
#include "device.h"
#include "renderpass.h"
#include "device/optix.h"

NAMESPACE_BEGIN(krr)

class GBufferPass : public RenderPass {
public:
	using SharedPtr = std::shared_ptr<GBufferPass>;
	KRR_REGISTER_PASS_DEC(GBufferPass);
	GBufferPass() = default;
	
	void initialize() override;
	void setScene(Scene::SharedPtr scene);
	void render(RenderContext *context);
	void renderUI() override;
	std::string getName() const override { return "GBufferPass"; }

private:
	OptixBackend::SharedPtr mOptixBackend;
	LaunchParameters <GBufferPass> mLaunchParams;

	bool mEnableDepth{};
	bool mEnableDiffuse{};
	bool mEnableSpecular{};
	bool mEnableNormal{};
	bool mEnableEmissive{};
	bool mEnableMotion{};
};

NAMESPACE_END(krr)