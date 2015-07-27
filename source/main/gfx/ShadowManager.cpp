/*
This source file is part of Rigs of Rods
Copyright 2005-2012 Pierre-Michel Ricordel
Copyright 2007-2012 Thomas Fischer

For more information, see http://www.rigsofrods.com/

Rigs of Rods is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3, as
published by the Free Software Foundation.

Rigs of Rods is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Rigs of Rods.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "ShadowManager.h"

#include "Ogre.h"
#include "OgreTerrain.h"

#include "Settings.h"

using namespace Ogre;

ShadowManager::ShadowManager()
{
	PSSM_Shadows.mPSSMSetup.setNull();
	PSSM_Shadows.mDepthShadows = false;
	PSSM_Shadows.ShadowsTextureNum = 3;
}

ShadowManager::~ShadowManager()
{
}

void ShadowManager::loadConfiguration()
{
	Ogre::String s = SSETTING("Shadow technique", "Parallel-split Shadow Maps");
	if (s == "Texture shadows")
		ShadowsType = SHADOWS_TEXTURE;
	else if (s == "Parallel-split Shadow Maps")
		ShadowsType = SHADOWS_PSSM;
	else
		ShadowsType = SHADOWS_NONE;

	updateShadowTechnique();
}

int ShadowManager::updateShadowTechnique()
{
	float shadowFarDistance = FSETTING("SightRange", 2000);
	float scoef = 0.12;
	gEnv->sceneManager->setShadowColour(Ogre::ColourValue(0.563 + scoef, 0.578 + scoef, 0.625 + scoef));
	gEnv->sceneManager->setShowDebugShadows(true);

	if (ShadowsType == SHADOWS_TEXTURE)
	{
		gEnv->sceneManager->setShadowFarDistance(shadowFarDistance);
		processTextureShadows();
	}
	else if (ShadowsType == SHADOWS_PSSM)
	{
		processPSSM();
		if (gEnv->sceneManager->getShowDebugShadows())
		{
			// add the overlay elements to show the shadow maps:
			// init overlay elements
			OverlayManager& mgr = Ogre::OverlayManager::getSingleton();
			Overlay* overlay = mgr.create("DebugOverlay");

			for (int i = 0; i < PSSM_Shadows.ShadowsTextureNum; ++i) {
				TexturePtr tex = gEnv->sceneManager->getShadowTexture(i);


				// Set up a debug panel to display the shadow
				MaterialPtr debugMat = MaterialManager::getSingleton().create("Ogre/DebugTexture" + StringConverter::toString(i), ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
				debugMat->getTechnique(0)->getPass(0)->setLightingEnabled(false);
				TextureUnitState *t = debugMat->getTechnique(0)->getPass(0)->createTextureUnitState(tex->getName());
				t->setTextureAddressingMode(TextureUnitState::TAM_CLAMP);

				OverlayContainer* debugPanel = (OverlayContainer*)(OverlayManager::getSingleton().createOverlayElement("Panel", "Ogre/DebugTexPanel" + StringConverter::toString(i)));
				debugPanel->_setPosition(0.8, i*0.25);
				debugPanel->_setDimensions(0.2, 0.24);
				debugPanel->setMaterialName(debugMat->getName());
				debugPanel->setEnabled(true);
				overlay->add2D(debugPanel);
				overlay->show();
			}
		}
	}
	return 0;
}

void ShadowManager::processTextureShadows()
{
	gEnv->sceneManager->setShadowTechnique(Ogre::SHADOWTYPE_TEXTURE_MODULATIVE);
	gEnv->sceneManager->setShadowTextureSettings(2048, 2);
}

void ShadowManager::processPSSM()
{
	gEnv->sceneManager->setShadowTechnique(Ogre::SHADOWTYPE_TEXTURE_ADDITIVE_INTEGRATED);

	gEnv->sceneManager->setShadowDirectionalLightExtrusionDistance(3000.0f);
	gEnv->sceneManager->setShadowFarDistance(1000.0f);
	gEnv->sceneManager->setShadowDirLightTextureOffset(0.7f);
	gEnv->sceneManager->setShadowTextureCountPerLightType(Ogre::Light::LT_DIRECTIONAL, PSSM_Shadows.ShadowsTextureNum);
	gEnv->sceneManager->setShadowTextureCount(PSSM_Shadows.ShadowsTextureNum);

	gEnv->sceneManager->setShadowTextureSelfShadow(true);
	gEnv->sceneManager->setShadowCasterRenderBackFaces(true);

	//Caster is set via materials

	if (PSSM_Shadows.mPSSMSetup.isNull())
	{
		// shadow camera setup
		Ogre::PSSMShadowCameraSetup* pssmSetup = new Ogre::PSSMShadowCameraSetup();

		pssmSetup->calculateSplitPoints(3, gEnv->mainCamera->getNearClipDistance(), gEnv->sceneManager->getShadowFarDistance(), 0.93f);
		pssmSetup->setSplitPadding(gEnv->mainCamera->getNearClipDistance());

		pssmSetup->setOptimalAdjustFactor(0, 5);
		pssmSetup->setOptimalAdjustFactor(1, 1);
		pssmSetup->setOptimalAdjustFactor(2, 0.5);

		PSSM_Shadows.mPSSMSetup.bind(pssmSetup);
		
		//Send split info to managed materials
		setManagedMaterialSplitPoints(pssmSetup->getSplitPoints());
	}
	gEnv->sceneManager->setShadowCameraSetup(PSSM_Shadows.mPSSMSetup);

	gEnv->sceneManager->setShadowTextureConfig(0, 2048, 2048, PF_FLOAT32_R);
	gEnv->sceneManager->setShadowTextureConfig(1, 1024, 1024, PF_FLOAT32_R);
	gEnv->sceneManager->setShadowTextureConfig(2, 1024, 1024, PF_FLOAT32_R);
}

void ShadowManager::updatePSSM()
{
	if (!PSSM_Shadows.mPSSMSetup.get())  return;
	//Ugh what here?
}

void ShadowManager::updateTerrainMaterial(Ogre::TerrainMaterialGeneratorA::SM2Profile* matProfile)
{
	if (ShadowsType == SHADOWS_PSSM)
	{
		Ogre::PSSMShadowCameraSetup* pssmSetup = static_cast<Ogre::PSSMShadowCameraSetup*>(PSSM_Shadows.mPSSMSetup.get());
		matProfile->setReceiveDynamicShadowsDepth(true);
		matProfile->setReceiveDynamicShadowsPSSM(pssmSetup);
	}
}

void ShadowManager::setManagedMaterialSplitPoints(Ogre::PSSMShadowCameraSetup::SplitPointList splitPointList)
{
	Ogre::Vector4 splitPoints;

	for (int i = 0; i < 3; ++i)
		splitPoints[i] = splitPointList[i];

	GpuSharedParametersPtr p = GpuProgramManager::getSingleton().getSharedParameters("pssm_params");
	p->setNamedConstant("pssmSplitPoints", splitPoints);
}
