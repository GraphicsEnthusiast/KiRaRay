#include "device/scene.h"
#include "device/optix.h"
#include "render/profiler/profiler.h"
#include "../scene.h"
#include "util/volume.h"

NAMESPACE_BEGIN(krr)

void RTScene::uploadManagedObject(SceneGraphLeaf::SharedPtr leaf, SceneObject object) {
	mManagedObjects[leaf] = object;
	object.getObjectData(leaf, true);
	cudaMemcpy(object.ptr(), object.data->data(), object.data->size(), cudaMemcpyHostToDevice);
}

void RTScene::updateManagedObject(SceneGraphLeaf::SharedPtr leaf) {
	if (mManagedObjects.find(leaf) != mManagedObjects.end()) {
		auto object = mManagedObjects[leaf];
		object.getObjectData(leaf, false);
		cudaMemcpyAsync(object.ptr(), object.data->data(), object.data->size(),
						cudaMemcpyHostToDevice, KRR_DEFAULT_STREAM);
	}
}

RTScene::RTScene(Scene::SharedPtr scene) : mScene(scene) {}

std::shared_ptr<Scene> RTScene::getScene() const { return mScene.lock(); }

void RTScene::uploadSceneData(const OptixSceneParameters &params) {
	// The order of upload is unchangeable since some of them are dependent to others.
	uploadSceneMaterialData();
	uploadSceneMediumData();
	uploadSceneMeshData();
	uploadSceneInstanceData();
	uploadSceneLightData();

	CUDA_SYNC_CHECK();
	if (params.buildMultilevel) 
		mOptixScene = std::make_shared<OptixSceneMultiLevel>(mScene.lock(), params);
	else mOptixScene = std::make_shared<OptixSceneSingleLevel>(mScene.lock(), params);
}

void RTScene::uploadSceneMeshData() {
	/* Upload mesh data to device... */
	auto &meshes = mScene.lock()->getMeshes();
	mMeshes.resize(meshes.size());
	for (size_t idx = 0; idx < meshes.size(); idx++) {
		const auto &mesh = meshes[idx];
		mMeshes[idx].positions.alloc_and_copy_from_host(mesh->positions);
		mMeshes[idx].normals.alloc_and_copy_from_host(mesh->normals);
		mMeshes[idx].texcoords.alloc_and_copy_from_host(mesh->texcoords);
		mMeshes[idx].tangents.alloc_and_copy_from_host(mesh->tangents);
		mMeshes[idx].indices.alloc_and_copy_from_host(mesh->indices);
		mMeshes[idx].material =
			mesh->getMaterial() ? &mMaterials[mesh->getMaterial()->getMaterialId()] : nullptr;
		if (mesh->inside) 
			mMeshes[idx].mediumInterface.inside = mMediumStorage.getPointer(mesh->inside);
		if (mesh->outside)
			mMeshes[idx].mediumInterface.outside = mMediumStorage.getPointer(mesh->outside);
	}
}

void RTScene::uploadSceneInstanceData() {
	/* Upload instance data to device... */
	auto &instances = mScene.lock()->getMeshInstances();
	mInstances.resize(instances.size());
	for (size_t idx = 0; idx < instances.size(); idx++) {
		uploadManagedObject(instances[idx], &mInstances[idx]);
	}
}

void RTScene::uploadSceneMaterialData() {
	/* Upload texture and material data to device... */
	auto &materials = mScene.lock()->getMaterials();
	mMaterials.resize(materials.size());
	for (auto idx = 0; idx < materials.size(); idx++) {
		uploadManagedObject(materials[idx], &mMaterials[idx]);
	}
}

void RTScene::uploadSceneLightData() {
	auto createTrianglePrimitives = [](Mesh::SharedPtr mesh, rt::InstanceData* instance) 
		-> std::vector<Triangle> {
		uint nTriangles = mesh->indices.size();
		std::vector<Triangle> triangles;
		for (uint i = 0; i < nTriangles; i++) 
			triangles.push_back(Triangle(i, instance));
		return triangles;
	};

	/* Process mesh lights (diffuse area lights).
	   Mesh lights do not actually exists in the scene graph, since rasterization does 
	   not inherently support them. We simply bypass them with storage in mesh data. */
	for (const auto &instance : mScene.lock()->getMeshInstances()) {
		const auto &mesh			   = instance->getMesh();
		const auto &material		   = mesh->getMaterial();
		if ((material && material->hasEmission()) || mesh->Le.any()) {
			rt::InstanceData &instanceData = mInstances[instance->getInstanceId()];
			for (size_t triId = 0; triId < mesh->indices.size(); triId++) 
				mLightStorage.addPointer(&instanceData.lights[triId]);
		}
	}

	/* Process other lights (environment lights and those analytical ones). */
	for (auto light : mScene.lock()->getLights()) {
		auto transform = light->getNode()->getGlobalTransform();
		float sceneRadius = mScene.lock()->getBoundingBox().diagonal().norm();
		if (auto infiniteLight = std::dynamic_pointer_cast<InfiniteLight>(light)) {
			mLightStorage.emplaceEntity<rt::InfiniteLight>(light);
			uploadManagedObject(light, mLightStorage.getPointer(light).cast<rt::InfiniteLight>());
		} else if (auto pointLight = std::dynamic_pointer_cast<PointLight>(light)) {
			mLightStorage.emplaceEntity<rt::PointLight>(light);
			uploadManagedObject(light, mLightStorage.getPointer(light).cast<rt::PointLight>());
		} else if (auto directionalLight = std::dynamic_pointer_cast<DirectionalLight>(light)) {
			mLightStorage.emplaceEntity<rt::DirectionalLight>(light);
			uploadManagedObject(light, mLightStorage.getPointer(light).cast<rt::DirectionalLight>());
		} else if (auto spotlight = std::dynamic_pointer_cast<SpotLight>(light)) {
			mLightStorage.emplaceEntity<rt::SpotLight>(light);
			uploadManagedObject(light, mLightStorage.getPointer(light).cast<rt::SpotLight>());
		} else {
			Log(Error, "Unsupported light type not uploaded to device memory.");
		}
	}
	
	mLightStorage.addPointers(mScene.lock()->getLights());

	/* Upload main constant light buffer and light sampler. */
	Log(Info, "A total of %zd light(s) processed!", mLightStorage.getPointers().size());
	if (!mLightStorage.getPointers().size())
		Log(Error, "There's no light source in the scene! "
			"Image will be dark, and may even cause crash...");
	auto lightSampler = UniformLightSampler(mLightStorage.getPointers());
	mLightSamplerBuffer.alloc_and_copy_from_host(&lightSampler, 1);
	// [Workaround] Since the area light hit depends on light buffer pointed from instance...
	CUDA_SYNC_CHECK();
}

void RTScene::uploadSceneMediumData() {
	// For a medium, its index in mediumBuffer is the same as medium->getMediumId();
	cudaDeviceSynchronize();
	for (auto medium : mScene.lock()->getMedia()) {
		if (auto m = std::dynamic_pointer_cast<HomogeneousVolume>(medium)) {
			mMediumStorage.emplaceEntity<HomogeneousMedium>(medium);
			uploadManagedObject(m, mMediumStorage.getPointer(m).cast<HomogeneousMedium>());
		} else if (auto m = std::dynamic_pointer_cast<VDBVolume>(medium)) {
			if (std::dynamic_pointer_cast<NanoVDBGrid<float>>(m->densityGrid)) {
				mMediumStorage.emplaceEntity<NanoVDBMedium<float>>(medium);
				uploadManagedObject(m, mMediumStorage.getPointer(m).cast<NanoVDBMedium<float>>());
			} else if (std::dynamic_pointer_cast<NanoVDBGrid<Array3f>>(m->densityGrid)) {
				mMediumStorage.emplaceEntity<NanoVDBMedium<Array3f>>(medium);
				uploadManagedObject(m, mMediumStorage.getPointer(m).cast<NanoVDBMedium<Array3f>>());
			} else {
				Log(Error, "Unsupported heterogeneous VDB medium data type");
				continue;
			}
		} else
			Log(Error, "Unknown medium type not uploaded to device memory.");
	}

	mMediumStorage.addPointers(mScene.lock()->getMedia());
	CUDA_SYNC_CHECK();
}

rt::SceneData RTScene::getSceneData() {
	rt::SceneData sceneData {};
	sceneData.meshes		 = mMeshes;
	sceneData.instances		 = mInstances;
	sceneData.materials		 = mMaterials;
	sceneData.lights		 = mLightStorage.getPointers();
	sceneData.infiniteLights = mLightStorage.getData<rt::InfiniteLight>();
	sceneData.lightSampler	 = mLightSamplerBuffer.data();
	return sceneData;
}

void RTScene::updateAccelStructure() { 
	mOptixScene->update();
}

// This routine should only be called by OptixBackend...
void RTScene::updateSceneData() {
	PROFILE("Update scene data");
	/* update managed objects, including most types of leaf nodes... */
	for (auto [leaf, object] : mManagedObjects) {
		assert(!leaf.expired());
		if (leaf.lock()->isUpdated()) {
			updateManagedObject(leaf.lock());
			leaf.lock()->setUpdated(false);
		}
	}
}

NAMESPACE_END(krr)