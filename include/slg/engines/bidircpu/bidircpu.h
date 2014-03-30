/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
 *                                                                         *
 * Licensed under the Apache License, Version 2.0 (the "License");         *
 * you may not use this file except in compliance with the License.        *
 * You may obtain a copy of the License at                                 *
 *                                                                         *
 *     http://www.apache.org/licenses/LICENSE-2.0                          *
 *                                                                         *
 * Unless required by applicable law or agreed to in writing, software     *
 * distributed under the License is distributed on an "AS IS" BASIS,       *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*
 * See the License for the specific language governing permissions and     *
 * limitations under the License.                                          *
 ***************************************************************************/

#ifndef _SLG_BIDIRCPU_H
#define	_SLG_BIDIRCPU_H

#include "slg/slg.h"
#include "slg/renderengine.h"

#include "luxrays/core/randomgen.h"
#include "slg/sampler/sampler.h"
#include "slg/film/film.h"
#include "slg/sdl/bsdf.h"
#include "slg/sdl/volume.h"

namespace slg {

//------------------------------------------------------------------------------
// Bidirectional path tracing CPU render engine
//------------------------------------------------------------------------------

typedef struct {
	BSDF bsdf;
	luxrays::Spectrum throughput;
	int depth;

	// Check Iliyan Georgiev's latest technical report for the details of how
	// MIS weight computation works (http://www.iliyan.com/publications/ImplementingVCM)
	float dVC; // MIS quantity used for vertex connection
	float dVCM; // MIS quantity used for vertex connection (and merging in a future)
	float dVM; // MIS quantity used for vertex merging

	// Volume rendering information
	PathVolumeInfo volInfo;
} PathVertexVM;

class BiDirCPURenderEngine;

class BiDirCPURenderThread : public CPUNoTileRenderThread {
public:
	BiDirCPURenderThread(BiDirCPURenderEngine *engine, const u_int index,
			luxrays::IntersectionDevice *device);

	friend class BiDirCPURenderEngine;

protected:
	// Used to offset Sampler data
	static const u_int sampleBootSize = 12;
	static const u_int sampleLightStepSize = 5;
	static const u_int sampleEyeStepSize = 10;

	static float MIS(const float a) {
		//return a; // Balance heuristic
		return a * a; // Power heuristic
	}

	virtual boost::thread *AllocRenderThread() { return new boost::thread(&BiDirCPURenderThread::RenderFunc, this); }

	void RenderFunc();

	void DirectLightSampling(
		const float u0, const float u1, const float u2,
		const float u3, const float u4,
		const PathVertexVM &eyeVertex, luxrays::Spectrum *radiance) const;
	void DirectHitLight(const bool finiteLightSource,
		const PathVertexVM &eyeVertex, luxrays::Spectrum *radiance) const;
	void DirectHitLight(const LightSource *light, const luxrays::Spectrum &lightRadiance,
		const float directPdfA, const float emissionPdfW,
		const PathVertexVM &eyeVertex, luxrays::Spectrum *radiance) const;

	void ConnectVertices(const PathVertexVM &eyeVertex, const PathVertexVM &BiDirVertex,
		SampleResult *eyeSampleResult, const float u0) const;
	void ConnectToEye(const PathVertexVM &BiDirVertex, const float u0,
		const luxrays::Point &lensPoint, vector<SampleResult> &sampleResults) const;

	void TraceLightPath(Sampler *sampler, const luxrays::Point &lensPoint,
		vector<PathVertexVM> &lightPathVertices,
		vector<SampleResult> &sampleResults) const;
	bool Bounce(Sampler *sampler, const u_int sampleOffset,
		PathVertexVM *pathVertex, luxrays::Ray *nextEventRay) const;

	u_int pixelCount;

	float misVmWeightFactor; // Weight of vertex merging (used in VC)
    float misVcWeightFactor; // Weight of vertex connection (used in VM)
	float vmNormalization; // 1 / (Pi * radius^2 * light_path_count)
};

class BiDirCPURenderEngine : public CPUNoTileRenderEngine {
public:
	BiDirCPURenderEngine(const RenderConfig *cfg, Film *flm, boost::mutex *flmMutex);

	RenderEngineType GetEngineType() const { return BIDIRCPU; }

	// Signed because of the delta parameter
	int maxEyePathDepth, maxLightPathDepth;

	// Used for vertex merging, it enables VM if it is > 0
	u_int lightPathsCount;
	float baseRadius; // VM (i.e. SPPM) start radius parameter
	float radiusAlpha; // VM (i.e. SPPM) alpha parameter

	int rrDepth;
	float rrImportanceCap;

	friend class BiDirCPURenderThread;

protected:
	virtual void StartLockLess();

private:
	CPURenderThread *NewRenderThread(const u_int index, luxrays::IntersectionDevice *device) {
		return new BiDirCPURenderThread(this, index, device);
	}	
};

}

#endif	/* _SLG_BIDIRCPU_H */
