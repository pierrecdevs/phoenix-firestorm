/**
 * @file apfloaterphototools.cpp
 * @brief Phototools floater
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * Copyright (C) 2025, William Weaver (paperwork) @ Second Life
 * Copyright (C) 2011, WoLf Loonie @ Second Life
 * Copyright (C) 2013, Zi Ree @ Second Life
 * Copyright (C) 2013, Ansariel Hiller @ Second Life
 * Copyright (C) 2013, Cinder Biscuits @ Me too
 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "apfloaterphototools.h"

#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "llfloaterpreference.h"
#include "llinventoryfunctions.h"
#include "llnotificationsutil.h"
#include "llsliderctrl.h"
#include "llspinctrl.h"
#include "lltrans.h"
#include "llviewercontrol.h"
#include "llviewernetwork.h" 

#include "llquaternion.h"
#include "llvirtualtrackball.h"
#include "llsky.h"
#include "llsettingssky.h"


#include "llcolorswatch.h"
#include "lltexturectrl.h"
#include "llenvironment.h"
#include "pipeline.h"

namespace
{
    // Atmospheric Colors
    const std::string FIELD_SKY_AMBIENT_LIGHT("ambient_light");
    const std::string FIELD_SKY_BLUE_HORIZON("blue_horizon");
    const std::string FIELD_SKY_BLUE_DENSITY("blue_density");

    // Atmosphere Settings
    const std::string   FIELD_SKY_HAZE_HORIZON("haze_horizon");
    const std::string   FIELD_SKY_HAZE_DENSITY("haze_density");
    const std::string   FIELD_SKY_DENSITY_MULTIP("density_multip");
    const std::string   FIELD_SKY_DISTANCE_MULTIP("distance_multip");
    const std::string   FIELD_SKY_MAX_ALT("max_alt");
    const std::string FIELD_SKY_SCENE_GAMMA("scene_gamma");
    // void updateGammaLabel();                    // FEA (I think this is UI for Brightness)
    const std::string FIELD_REFLECTION_PROBE_AMBIANCE("probe_ambiance");

    // Rainbow and Halo Settings
    const std::string   FIELD_SKY_DENSITY_MOISTURE_LEVEL("moisture_level");
    const std::string   FIELD_SKY_DENSITY_DROPLET_RADIUS("droplet_radius");
    const std::string   FIELD_SKY_DENSITY_ICE_LEVEL("ice_level");

    // Cloud Settings
    const std::string FIELD_SKY_CLOUD_COLOR("cloud_color");
    const std::string FIELD_SKY_CLOUD_COVERAGE("cloud_coverage");
    const std::string FIELD_SKY_CLOUD_SCALE("cloud_scale");
    const std::string   FIELD_SKY_CLOUD_VARIANCE("cloud_variance");
    const std::string   FIELD_SKY_CLOUD_DENSITY_X("cloud_density_x");
    const std::string   FIELD_SKY_CLOUD_DENSITY_Y("cloud_density_y");
    const std::string   FIELD_SKY_CLOUD_DENSITY_D("cloud_density_d");
    const std::string   FIELD_SKY_CLOUD_DETAIL_X("cloud_detail_x");
    const std::string   FIELD_SKY_CLOUD_DETAIL_Y("cloud_detail_y");
    const std::string   FIELD_SKY_CLOUD_DETAIL_D("cloud_detail_d");
    const std::string   FIELD_SKY_CLOUD_SCROLL_XY("cloud_scroll_xy");
    const std::string FIELD_SKY_CLOUD_MAP("cloud_map");

    // Sun and Moon Colors
    const std::string FIELD_SKY_SUN_COLOR("sun_moon_color");

    // Sun and Stars Settings
    const std::string   FIELD_SKY_SUN_IMAGE("sun_image");
    const std::string FIELD_SKY_GLOW_FOCUS("glow_focus");
    const std::string FIELD_SKY_GLOW_SIZE("glow_size");
    const std::string FIELD_SKY_STAR_BRIGHTNESS("star_brightness");
    const std::string FIELD_SKY_SUN_SCALE("sun_scale");
    const std::string FIELD_SKY_SUN_AZIMUTH("sun_azimuth");
    const std::string FIELD_SKY_SUN_ELEVATION("sun_elevation");
    
    // Moon Settings
    const std::string   FIELD_SKY_MOON_IMAGE("moon_image");
    const std::string   FIELD_SKY_MOON_SCALE("moon_scale");
    const std::string   FIELD_SKY_MOON_BRIGHTNESS("moon_brightness");
    const std::string FIELD_SKY_MOON_AZIMUTH("moon_azimuth");
    const std::string FIELD_SKY_MOON_ELEVATION("moon_elevation");

    const F32 SLIDER_SCALE_SUN_AMBIENT(3.0f);
    const F32 SLIDER_SCALE_BLUE_HORIZON_DENSITY(2.0f);
    const F32 SLIDER_SCALE_GLOW_R(20.0f);
    const F32 SLIDER_SCALE_GLOW_B(-5.0f);
    const F32 SLIDER_SCALE_DENSITY_MULTIPLIER(0.001f);

    // const S32 FLOATER_ENVIRONMENT_UPDATE(-2);
    // const std::string FIELD_WATER_NORMAL_MAP("water_normal_map");
    // const std::string FIELD_SKY_SUN_ROTATION("sun_rotation");
    // const std::string FIELD_SKY_MOON_ROTATION("moon_rotation");
    // const std::string BTN_RESET("btn_reset");
}

class APSettingsCollector : public LLInventoryCollectFunctor
{
public:
    APSettingsCollector()
    {
        // No folder filtering in the collector itself anymore
    }

    virtual ~APSettingsCollector() {}

    bool operator()(LLInventoryCategory* cat, LLInventoryItem* item)
    {
        if (item && item->getType() == LLAssetType::AT_SETTINGS)
        {
            // <AP:WW> Collect all settings types now, filtering in loadPresets()
            if (mSeen.find(item->getAssetUUID()) == mSeen.end())
            {
                mSeen.insert(item->getAssetUUID());
                return true; // Include the item if it's a settings type
            }
        }
        return false; // Exclude the item otherwise
    }

protected:
    std::set<LLUUID> mSeen;
};

APFloaterPhototools::APFloaterPhototools(const LLSD& key)
:   LLFloater(key),
    mEnvChangedConnection()
{

}

APFloaterPhototools::~APFloaterPhototools()
{
    if (mEnvChangedConnection.connected())
    {
        mEnvChangedConnection.disconnect();
    }
}

void APFloaterPhototools::onOpen(const LLSD& key)
{
    loadPresets();
    setSelectedEnvironment();
    LLAvatarComplexityControls::setIndirectMaxNonImpostors();
    LLAvatarComplexityControls::setIndirectMaxArc();
    LLAvatarComplexityControls::setText(gSavedSettings.getU32("RenderAvatarMaxComplexity"), mMaxComplexityLabel);

    // if (!mLiveSky)
    // {
    //    LLEnvironment::instance().saveBeaconsState();
    // }
    // captureCurrentEnvironment();

    // mEnvChangedConnection = LLEnvironment::instance().setEnvironmentChanged([this](LLEnvironment::EnvSelection_t env, S32 version){ onEnvironmentUpdated(env, version); });

    // mEnvChangedConnection = LLEnvironment::instance().setEnvironmentChanged([this](LLEnvironment::EnvSelection_t env, S32 version){ setSelectedEnvironment(); });
    
    // HACK -- resume reflection map manager because "setEnvironmentChanged" may pause it (SL-20456)
    gPipeline.mReflectionMapManager.resume();

    // refreshSky();
}

void APFloaterPhototools::initCallbacks()
{
    gSavedSettings.getControl("APPhototoolsShowOnlyMySettings")->getSignal()->connect(boost::bind(&APFloaterPhototools::onShowMySettingsChanged, this));
    
    getChild<LLUICtrl>("WLPresetsCombo")->setCommitCallback(boost::bind(&APFloaterPhototools::onChangeSkyPreset, this));
    getChild<LLUICtrl>("WLPrevPreset")->setCommitCallback(boost::bind(&APFloaterPhototools::onClickSkyPrev, this));
    getChild<LLUICtrl>("WLNextPreset")->setCommitCallback(boost::bind(&APFloaterPhototools::onClickSkyNext, this));

    getChild<LLUICtrl>("WaterPresetsCombo")->setCommitCallback(boost::bind(&APFloaterPhototools::onChangeWaterPreset, this));
    getChild<LLUICtrl>("WWPrevPreset")->setCommitCallback(boost::bind(&APFloaterPhototools::onClickWaterPrev, this));
    getChild<LLUICtrl>("WWNextPreset")->setCommitCallback(boost::bind(&APFloaterPhototools::onClickWaterNext, this));
    
    // getChild<LLUICtrl>("DCPresetsCombo")->setCommitCallback(boost::bind(&APFloaterPhototools::onChangeDayCyclePreset, this));
    // getChild<LLUICtrl>("DCPrevPreset")->setCommitCallback(boost::bind(&APFloaterPhototools::onClickDayCyclePrev, this));
    // getChild<LLUICtrl>("DCNextPreset")->setCommitCallback(boost::bind(&APFloaterPhototools::onClickDayCycleNext, this));

    gSavedSettings.getControl("RenderShadowSplitExponent")->getSignal()->connect(boost::bind(&APFloaterPhototools::refreshSettings, this));
    
    getChild<LLSlider>("SB_Shd_Clarity")->setCommitCallback(boost::bind(&APFloaterPhototools::onChangeRenderShadowSplitExponentSliderY, this));
    getChild<LLSpinCtrl>("S_Shd_Clarity")->setCommitCallback(boost::bind(&APFloaterPhototools::onChangeRenderShadowSplitExponentSpinnerY, this));
    getChild<LLButton>("Shd_Clarity_Reset")->setCommitCallback(boost::bind(&APFloaterPhototools::onClickResetRenderShadowSplitExponentY, this));

    gSavedSettings.getControl("RenderShadowGaussian")->getSignal()->connect(boost::bind(&APFloaterPhototools::refreshSettings, this));
    
    getChild<LLSlider>("SB_Shd_Soften")->setCommitCallback(boost::bind(&APFloaterPhototools::onChangeRenderShadowGaussianSlider, this));
    getChild<LLSpinCtrl>("S_Shd_Soften")->setCommitCallback(boost::bind(&APFloaterPhototools::onChangeRenderShadowGaussianSpinner, this));
    getChild<LLButton>("Reset_Shd_Soften")->setCommitCallback(boost::bind(&APFloaterPhototools::onClickResetRenderShadowGaussianX, this));

    getChild<LLSlider>("SB_AO_Soften")->setCommitCallback(boost::bind(&APFloaterPhototools::onChangeRenderShadowGaussianSlider, this));
    getChild<LLSpinCtrl>("S_AO_Soften")->setCommitCallback(boost::bind(&APFloaterPhototools::onChangeRenderShadowGaussianSpinner, this));
    getChild<LLButton>("Reset_AO_Soften")->setCommitCallback(boost::bind(&APFloaterPhototools::onClickResetRenderShadowGaussianY, this));

    gSavedSettings.getControl("RenderSSAOEffect")->getSignal()->connect(boost::bind(&APFloaterPhototools::refreshSettings, this));

    getChild<LLSlider>("SB_Effect")->setCommitCallback(boost::bind(&APFloaterPhototools::onChangeRenderSSAOEffectSlider, this));
    getChild<LLSpinCtrl>("S_Effect")->setCommitCallback(boost::bind(&APFloaterPhototools::onChangeRenderSSAOEffectSpinner, this));
    getChild<LLButton>("Reset_Effect")->setCommitCallback(boost::bind(&APFloaterPhototools::onClickResetRenderSSAOEffectX, this));

    getChild<LLSlider>("SB_Saturation")->setCommitCallback(boost::bind(&APFloaterPhototools::onChangeRenderSSAOEffectSliderY, this));
    getChild<LLSpinCtrl>("S_Saturation")->setCommitCallback(boost::bind(&APFloaterPhototools::onChangeRenderSSAOEffectSpinnerY, this));
    getChild<LLButton>("Reset_Saturation")->setCommitCallback(boost::bind(&APFloaterPhototools::onClickResetRenderSSAOEffectY, this));

    mMaxComplexitySlider->setCommitCallback(boost::bind(&APFloaterPhototools::updateMaxComplexity, this));
    gSavedSettings.getControl("RenderAvatarMaxComplexity")->getCommitSignal()->connect(boost::bind(&APFloaterPhototools::updateMaxComplexityLabel, this, _2));
    gSavedSettings.getControl("IndirectMaxNonImpostors")->getCommitSignal()->connect(boost::bind(&APFloaterPhototools::updateMaxNonImpostors, this, _2));

    mEnvChangedConnection = LLEnvironment::instance().setEnvironmentChanged(
        [this](LLEnvironment::EnvSelection_t env, S32 version)
        {
            setSelectedEnvironment(); 
            refreshSky();
        });
}

bool APFloaterPhototools::postBuild()
{
    mShowMySettingsCheckbox = getChild<LLCheckBoxCtrl>("show_my_settings_checkbox"); 

    mWaterPresetsCombo = getChild<LLComboBox>("WaterPresetsCombo");
    mWLPresetsCombo = getChild<LLComboBox>("WLPresetsCombo");
    /// mDayCyclePresetsCombo = getChild<LLComboBox>("DCPresetsCombo");
 
    mSliderRenderShadowSplitExponentY = getChild<LLSlider>("SB_Shd_Clarity");
    mSpinnerRenderShadowSplitExponentY = getChild<LLSpinCtrl>("S_Shd_Clarity");

    mSliderRenderShadowGaussianX = getChild<LLSlider>("SB_Shd_Soften");
    mSpinnerRenderShadowGaussianX = getChild<LLSpinCtrl>("S_Shd_Soften");

    mSliderRenderShadowGaussianY = getChild<LLSlider>("SB_AO_Soften");
    mSpinnerRenderShadowGaussianY = getChild<LLSpinCtrl>("S_AO_Soften");

    mSliderRenderSSAOEffectX = getChild<LLSlider>("SB_Effect");
    mSpinnerRenderSSAOEffectX = getChild<LLSpinCtrl>("S_Effect");
    
    mSliderRenderSSAOEffectY = getChild<LLSlider>("SB_Saturation");
    mSpinnerRenderSSAOEffectY = getChild<LLSpinCtrl>("S_Saturation");
    
    mMaxComplexitySlider = getChild<LLSliderCtrl>("IndirectMaxComplexity2");
    mMaxComplexityLabel = getChild<LLTextBox>("IndirectMaxComplexityText2");

    // Atmospheric Colors
    getChild<LLUICtrl>(FIELD_SKY_AMBIENT_LIGHT)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onAmbientLightChanged(); });
    getChild<LLUICtrl>(FIELD_SKY_BLUE_HORIZON)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onBlueHorizonChanged(); });
    getChild<LLUICtrl>(FIELD_SKY_BLUE_DENSITY)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onBlueDensityChanged(); });

    // Atmosphere Settings
    getChild<LLUICtrl>(FIELD_SKY_HAZE_HORIZON)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onHazeHorizonChanged(); });
    getChild<LLUICtrl>(FIELD_SKY_HAZE_DENSITY)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onHazeDensityChanged(); });
    getChild<LLUICtrl>(FIELD_SKY_DENSITY_MULTIP)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onDensityMultipChanged(); });
    getChild<LLUICtrl>(FIELD_SKY_DISTANCE_MULTIP)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onDistanceMultipChanged(); });
    getChild<LLUICtrl>(FIELD_SKY_MAX_ALT)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onMaxAltChanged(); });
    getChild<LLUICtrl>(FIELD_SKY_SCENE_GAMMA)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onSceneGammaChanged(); });
    getChild<LLUICtrl>(FIELD_REFLECTION_PROBE_AMBIANCE)->setCommitCallback([this](LLUICtrl*, const LLSD&) { onReflectionProbeAmbianceChanged(); });

    // Rainbow and Halo Settings
    getChild<LLUICtrl>(FIELD_SKY_DENSITY_MOISTURE_LEVEL)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onMoistureLevelChanged(); });
    getChild<LLUICtrl>(FIELD_SKY_DENSITY_DROPLET_RADIUS)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onDropletRadiusChanged(); });
    getChild<LLUICtrl>(FIELD_SKY_DENSITY_ICE_LEVEL)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onIceLevelChanged(); });

    // Cloud Settings
    getChild<LLUICtrl>(FIELD_SKY_CLOUD_COLOR)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onCloudColorChanged(); });
    
    getChild<LLUICtrl>(FIELD_SKY_CLOUD_COVERAGE)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onCloudCoverageChanged(); });
    getChild<LLUICtrl>(FIELD_SKY_CLOUD_SCALE)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onCloudScaleChanged(); });
    getChild<LLUICtrl>(FIELD_SKY_CLOUD_VARIANCE)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onCloudVarianceChanged(); });
    
    getChild<LLUICtrl>(FIELD_SKY_CLOUD_DENSITY_X)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onCloudDensityChanged(); });
    getChild<LLUICtrl>(FIELD_SKY_CLOUD_DENSITY_Y)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onCloudDensityChanged(); });
    getChild<LLUICtrl>(FIELD_SKY_CLOUD_DENSITY_D)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onCloudDensityChanged(); });
    
    getChild<LLUICtrl>(FIELD_SKY_CLOUD_DETAIL_X)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onCloudDetailChanged(); });
    getChild<LLUICtrl>(FIELD_SKY_CLOUD_DETAIL_Y)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onCloudDetailChanged(); });
    getChild<LLUICtrl>(FIELD_SKY_CLOUD_DETAIL_D)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onCloudDetailChanged(); });
    
    getChild<LLUICtrl>(FIELD_SKY_CLOUD_SCROLL_XY)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onCloudScrollChanged(); });
    
    getChild<LLTextureCtrl>(FIELD_SKY_CLOUD_MAP)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onCloudMapChanged(); });
    getChild<LLTextureCtrl>(FIELD_SKY_CLOUD_MAP)->setDefaultImageAssetID(LLSettingsSky::GetDefaultCloudNoiseTextureId());
    getChild<LLTextureCtrl>(FIELD_SKY_CLOUD_MAP)->setAllowNoTexture(true);

    // Sun and Moon Colors
    getChild<LLUICtrl>(FIELD_SKY_SUN_COLOR)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onSunColorChanged(); });

    // Sun and Stars Settings
    getChild<LLUICtrl>(FIELD_SKY_SUN_IMAGE)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onSunImageChanged(); });
    getChild<LLTextureCtrl>(FIELD_SKY_SUN_IMAGE)->setBlankImageAssetID(LLSettingsSky::GetBlankSunTextureId());
    getChild<LLTextureCtrl>(FIELD_SKY_SUN_IMAGE)->setDefaultImageAssetID(LLSettingsSky::GetBlankSunTextureId());
    getChild<LLTextureCtrl>(FIELD_SKY_SUN_IMAGE)->setAllowNoTexture(true);
    
    getChild<LLUICtrl>(FIELD_SKY_GLOW_FOCUS)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onGlowChanged(); });
    getChild<LLUICtrl>(FIELD_SKY_GLOW_SIZE)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onGlowChanged(); });
    getChild<LLUICtrl>(FIELD_SKY_STAR_BRIGHTNESS)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onStarBrightnessChanged(); });
    getChild<LLUICtrl>(FIELD_SKY_SUN_SCALE)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onSunScaleChanged(); });
    getChild<LLUICtrl>(FIELD_SKY_SUN_AZIMUTH)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onSunAzimElevChanged(); });
    getChild<LLUICtrl>(FIELD_SKY_SUN_ELEVATION)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onSunAzimElevChanged(); });
    
    // Moon Settings
    getChild<LLUICtrl>(FIELD_SKY_MOON_IMAGE)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onMoonImageChanged(); });
    getChild<LLTextureCtrl>(FIELD_SKY_MOON_IMAGE)->setDefaultImageAssetID(LLSettingsSky::GetDefaultMoonTextureId());
    getChild<LLTextureCtrl>(FIELD_SKY_MOON_IMAGE)->setBlankImageAssetID(LLSettingsSky::GetDefaultMoonTextureId());
    getChild<LLTextureCtrl>(FIELD_SKY_MOON_IMAGE)->setAllowNoTexture(true);
    
    getChild<LLUICtrl>(FIELD_SKY_MOON_SCALE)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onMoonScaleChanged(); });
    getChild<LLUICtrl>(FIELD_SKY_MOON_BRIGHTNESS)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onMoonBrightnessChanged(); });
    getChild<LLUICtrl>(FIELD_SKY_MOON_AZIMUTH)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onMoonAzimElevChanged(); });
    getChild<LLUICtrl>(FIELD_SKY_MOON_ELEVATION)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onMoonAzimElevChanged(); });

    // getChild<LLUICtrl>(FIELD_SKY_SUN_ROTATION)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onSunRotationChanged(); });
    // getChild<LLUICtrl>(FIELD_SKY_MOON_ROTATION)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onMoonRotationChanged(); });
    // getChild<LLUICtrl>(BTN_RESET)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onButtonReset(); });
    // getChild<LLTextureCtrl>(FIELD_WATER_NORMAL_MAP)->setDefaultImageAssetID(LLSettingsWater::GetDefaultWaterNormalAssetId());
    // getChild<LLTextureCtrl>(FIELD_WATER_NORMAL_MAP)->setBlankImageAssetID(BLANK_OBJECT_NORMAL);
    // getChild<LLTextureCtrl>(FIELD_WATER_NORMAL_MAP)->setCommitCallback([this](LLUICtrl *, const LLSD &) { onWaterMapChanged(); });

    refreshSettings(); 
    
    refreshSky();

    initCallbacks();

    return LLFloater::postBuild();
}

void APFloaterPhototools::onClose(bool app_quitting)
 {
     
}

// Environment Tab

void APFloaterPhototools::onShowMySettingsChanged()
{
    if (mShowMySettingsCheckbox)
    {

    }
    loadPresets(); // Refresh the preset lists
}

void APFloaterPhototools::loadPresets()
{
    LLInventoryModel::cat_array_t cats;
    LLInventoryModel::item_array_t items;
    APSettingsCollector collector;
    gInventory.collectDescendentsIf(LLUUID::null,
                                    cats,
                                    items,
                                    LLInventoryModel::EXCLUDE_TRASH,
                                    collector);

    std::multimap<std::string, LLUUID> sky_map;
    std::multimap<std::string, LLUUID> water_map;
    //  std::multimap<std::string, LLUUID> daycycle_map;

    // <AP:WW> Get checkbox state for folder filtering
    bool showMySettingsFolderOnly = false;
    if (mShowMySettingsCheckbox) // Check if the checkbox control is valid
    {
        showMySettingsFolderOnly = mShowMySettingsCheckbox->getValue();
    }

    LLUUID settingsFolderUUID = LLUUID::null;
    if (showMySettingsFolderOnly) // <AP:WW> Only find FT_SETTINGS folder if checkbox is checked
    {
        settingsFolderUUID = gInventory.findCategoryUUIDForType(LLFolderType::FT_SETTINGS);
        if (settingsFolderUUID.isNull())
        {
            LL_WARNS("PresetDebug") << "<AP:WW> WARNING: Could not find FT_SETTINGS folder in inventory!  Showing all settings." << LL_ENDL;
            showMySettingsFolderOnly = false; // Fallback to showing all if folder not found
        }
    }

    for (LLInventoryModel::item_array_t::const_iterator it = items.begin(); it != items.end(); ++it)
    {
        LLInventoryItem* item = *it;

        // <AP:WW> Conditional filtering based on "Show Only FT_Settings Folder" checkbox
        bool shouldIncludeItem = true; // By default, include

        if (showMySettingsFolderOnly)
        {
            if (!gInventory.isObjectDescendentOf(item->getUUID(), settingsFolderUUID))
            {
                shouldIncludeItem = false; // Exclude if "Show Only FT_Settings" is checked AND not in FT_SETTINGS folder
            }
        }

        if (shouldIncludeItem)
        {
            LLSettingsType::type_e type = LLSettingsType::fromInventoryFlags(item->getFlags());
            switch (type)
            {
                case LLSettingsType::ST_SKY:
                    sky_map.insert(std::make_pair(item->getName(), item->getAssetUUID()));
                    break;
                case LLSettingsType::ST_WATER:
                    water_map.insert(std::make_pair(item->getName(), item->getAssetUUID()));
                    break;
                case LLSettingsType::ST_DAYCYCLE:
                    // Intentionally ignoring Day Cycle settings in Phototools
                    break; // Add this case back, but leave it empty
                default:
                    // This will now only catch genuinely unknown/unexpected setting types
                    LL_WARNS() << "Found unhandled setting type for item: " << item->getName() << LL_ENDL;
                    break;
            }
        }
    }

    loadWaterPresets(water_map);
    loadSkyPresets(sky_map);
    // loadDayCyclePresets(daycycle_map);
}

// void APFloaterPhototools::loadDayCyclePresets(const std::multimap<std::string, LLUUID>& daycycle_map)
// {
    // mDayCyclePresetsCombo->operateOnAll(LLComboBox::OP_DELETE);
    // mDayCyclePresetsCombo->add(LLTrans::getString("QP_WL_Region_Default"), LLSD(PRESET_NAME_REGION_DEFAULT), EAddPosition::ADD_BOTTOM, false);
    // mDayCyclePresetsCombo->add(LLTrans::getString("QP_WL_None"), LLSD(PRESET_NAME_NONE), EAddPosition::ADD_BOTTOM, false);
    // mDayCyclePresetsCombo->addSeparator();

    // Add setting presets.
    // for (std::multimap<std::string, LLUUID>::const_iterator it = daycycle_map.begin(); it != daycycle_map.end(); ++it)
    // {
        // const std::string& preset_name = (*it).first;
        // const LLUUID& asset_id = (*it).second;

        // if (!preset_name.empty())
        // {
            // mDayCyclePresetsCombo->add(preset_name, LLSD(asset_id));
        // }
    // }
// <FS:Beq> Opensim legacy windlight support
// Opensim may support both environment and extenvironment caps on the same region
// we also need these disabled in SL on the OpenSim build.
// #ifdef OPENSIM
    // if(LLGridManager::getInstance()->isInOpenSim())
    // {
        // LL_DEBUGS("WindlightCaps") << "Adding legacy day cycle presets to QP" << LL_ENDL;
        // WL still supported
        // if (!daycycle_map.empty() && !LLEnvironment::getInstance()->mLegacyDayCycles.empty())
        // {
            // mDayCyclePresetsCombo->addSeparator();
        // }
        // for(const auto& preset_name : LLEnvironment::getInstance()->mLegacyDayCycles)
        // {
            // we add by name and only build the envp on demand
            // LL_DEBUGS("WindlightCaps") << "Adding legacy day cycle " << preset_name << LL_ENDL;
            // mDayCyclePresetsCombo->add(preset_name, LLSD(preset_name));
        // }
        // LL_DEBUGS("WindlightCaps") << "Done: Adding legacy day cycle presets to QP" << LL_ENDL;
    // }
// #endif
// }

void APFloaterPhototools::loadSkyPresets(const std::multimap<std::string, LLUUID>& sky_map)
{
    mWLPresetsCombo->operateOnAll(LLComboBox::OP_DELETE);
    mWLPresetsCombo->add(LLTrans::getString("QP_WL_Region_Default"), LLSD(PRESET_NAME_REGION_DEFAULT), EAddPosition::ADD_BOTTOM, false);
    mWLPresetsCombo->add(LLTrans::getString("QP_WL_Day_Cycle_Based"), LLSD(PRESET_NAME_DAY_CYCLE), EAddPosition::ADD_BOTTOM, false);
    mWLPresetsCombo->addSeparator();

    // Add setting presets.
    for (std::multimap<std::string, LLUUID>::const_iterator it = sky_map.begin(); it != sky_map.end(); ++it)
    {
        const std::string& preset_name = (*it).first;
        const LLUUID& asset_id = (*it).second;

        if (!preset_name.empty())
        {
            mWLPresetsCombo->add(preset_name, LLSD(asset_id));
        }
    }
// <FS:Beq> Opensim legacy windlight support
// Opensim may support both environment and extenvironment caps on the same region
// we also need these disabled in SL on the OpenSim build.
#ifdef OPENSIM
    if(LLGridManager::getInstance()->isInOpenSim())
    {
        LL_DEBUGS("WindlightCaps") << "Adding legacy sky presets to QP" << LL_ENDL;
        // WL still supported
        if (!sky_map.empty() && !LLEnvironment::getInstance()->mLegacySkies.empty())
        {
            mWLPresetsCombo->addSeparator();
        }
        for(const auto& preset_name : LLEnvironment::getInstance()->mLegacySkies)
        {
            // we add by name and only build the envp on demand
            LL_DEBUGS("WindlightCaps") << "Adding legacy sky " << preset_name << LL_ENDL;
            // append "WL" to denote legacy. Have to create a new string not update the reference.
            mWLPresetsCombo->add(preset_name+ "[WL]", LLSD(preset_name));
        }
        LL_DEBUGS("WindlightCaps") << "Done: Adding legacy sky presets to QP" << LL_ENDL;
    }
#endif
}

void APFloaterPhototools::loadWaterPresets(const std::multimap<std::string, LLUUID>& water_map)
{
    mWaterPresetsCombo->operateOnAll(LLComboBox::OP_DELETE);
    mWaterPresetsCombo->add(LLTrans::getString("QP_WL_Region_Default"), LLSD(PRESET_NAME_REGION_DEFAULT), EAddPosition::ADD_BOTTOM, false);
    mWaterPresetsCombo->add(LLTrans::getString("QP_WL_Day_Cycle_Based"), LLSD(PRESET_NAME_DAY_CYCLE), EAddPosition::ADD_BOTTOM, false);
    mWaterPresetsCombo->addSeparator();

    // Add setting presets.
    for (std::multimap<std::string, LLUUID>::const_iterator it = water_map.begin(); it != water_map.end(); ++it)
    {
        const std::string& preset_name = (*it).first;
        const LLUUID& asset_id = (*it).second;

        if (!preset_name.empty())
        {
            mWaterPresetsCombo->add(preset_name, LLSD(asset_id));
        }
    }
// <FS:Beq> Opensim legacy windlight support
// Opensim may support both environment and extenvironment caps on the same region
// we also need these disabled in SL on the OpenSim build.
#ifdef OPENSIM
    if(LLGridManager::getInstance()->isInOpenSim())
    {
        LL_DEBUGS("WindlightCaps") << "Adding legacy presets to QP" << LL_ENDL;
        // WL still supported
        if (!water_map.empty() && !LLEnvironment::getInstance()->mLegacyWater.empty())
        {
            mWaterPresetsCombo->addSeparator();
        }
        for(const auto& preset_name : LLEnvironment::getInstance()->mLegacyWater)
        {
            // we add by name and only build the envp on demand
            LL_DEBUGS("WindlightCaps") << "Adding legacy water " << preset_name << LL_ENDL;
            mWaterPresetsCombo->add(preset_name, LLSD(preset_name));
        }
        LL_DEBUGS("WindlightCaps") << "Done: Adding legacy water presets to QP" << LL_ENDL;
    }
#endif
}

void APFloaterPhototools::setDefaultPresetsEnabled(bool enabled)
{
    LLScrollListItem* item{ nullptr };

    item = mWLPresetsCombo->getItemByValue(LLSD(PRESET_NAME_REGION_DEFAULT));
    if (item) item->setEnabled(enabled);

    item = mWLPresetsCombo->getItemByValue(LLSD(PRESET_NAME_DAY_CYCLE));
    if (item) item->setEnabled(enabled);

    item = mWaterPresetsCombo->getItemByValue(LLSD(PRESET_NAME_REGION_DEFAULT));
    if (item) item->setEnabled(enabled);

    item = mWaterPresetsCombo->getItemByValue(LLSD(PRESET_NAME_DAY_CYCLE));
    if (item) item->setEnabled(enabled);

    // item = mDayCyclePresetsCombo->getItemByValue(LLSD(PRESET_NAME_REGION_DEFAULT));
    // if (item) item->setEnabled(enabled);

    // item = mDayCyclePresetsCombo->getItemByValue(LLSD(PRESET_NAME_NONE));
    // if (item) item->setEnabled(enabled);
}

void APFloaterPhototools::setSelectedEnvironment()
{
    //LL_INFOS() << "EEP: getSelectedEnvironment: " << LLEnvironment::instance().getSelectedEnvironment() << LL_ENDL;

    setDefaultPresetsEnabled(true);
    mWLPresetsCombo->selectByValue(LLSD(PRESET_NAME_REGION_DEFAULT));
    mWaterPresetsCombo->selectByValue(LLSD(PRESET_NAME_REGION_DEFAULT));
    // mDayCyclePresetsCombo->selectByValue(LLSD(PRESET_NAME_REGION_DEFAULT));

    if (LLEnvironment::instance().getSelectedEnvironment() == LLEnvironment::ENV_LOCAL)
    {
        // Day cycle, fixed sky and fixed water may all be set at the same time
        // Check and set day cycle first. Fixed sky and water both override
        // the sky and water settings in a day cycle, so check them after the
        // day cycle. If no fixed sky or fixed water is set, they are either
        // defined in the day cycle or inherited from a higher environment level.
        LLSettingsDay::ptr_t day = LLEnvironment::instance().getEnvironmentDay(LLEnvironment::ENV_LOCAL);
        if (day)
        {
            //LL_INFOS() << "EEP: day name = " << day->getName() << " - asset id = " << day->getAssetId() << LL_ENDL;
            if( day->getAssetId().notNull())
            { // EEP processing
                // mDayCyclePresetsCombo->selectByValue(LLSD(day->getAssetId()));
                // Sky and Water are part of a day cycle in EEP
                mWLPresetsCombo->selectByValue(LLSD(PRESET_NAME_DAY_CYCLE));
                mWaterPresetsCombo->selectByValue(LLSD(PRESET_NAME_DAY_CYCLE));
            }
#ifdef OPENSIM
            else if (LLGridManager::getInstance()->isInOpenSim())
            {
                auto preset_name = day->getName();
                LL_DEBUGS("WindlightCaps") << "Current Day cycle is " << preset_name << LL_ENDL;
                if (preset_name == "_default_")
                {
                    preset_name = "Default";
                }
                // mDayCyclePresetsCombo->selectByValue(preset_name);
                // Sky is part of day so treat that as day cycle
                mWLPresetsCombo->selectByValue(LLSD(PRESET_NAME_DAY_CYCLE));
                // Water is not part of legacy day so we need to hunt around
                LLSettingsWater::ptr_t water = LLEnvironment::instance().getEnvironmentFixedWater(LLEnvironment::ENV_LOCAL);
                if (water)
                {
                    // This is going to be possible. OS will support both Legacy and EEP
                    // so having a water EEP asset with a Legacy day cycle could happen.
                    LLUUID asset_id = water->getAssetId();
                    if (asset_id.notNull())
                    {
                        mWaterPresetsCombo->selectByValue(LLSD(asset_id));
                    }
                    else
                    {
                        std::string preset_name = water->getName();
                        if (preset_name == "_default_")
                        {
                            preset_name = "Default";
                        }
                        mWaterPresetsCombo->selectByValue(preset_name);
                    }
                }
            }
#endif //OPENSIM
        }
        else
        {
            // mDayCyclePresetsCombo->selectByValue(LLSD(PRESET_NAME_NONE));
        }

        LLSettingsSky::ptr_t sky = LLEnvironment::instance().getEnvironmentFixedSky(LLEnvironment::ENV_LOCAL);
        if (sky)
        {
            //LL_INFOS() << "EEP: sky name = " << sky->getName() << " - asset id = " << sky->getAssetId() << LL_ENDL;
            if(sky->getAssetId().notNull())
            {
                mWLPresetsCombo->selectByValue(LLSD(sky->getAssetId()));
            }
#ifdef OPENSIM
            else if (LLGridManager::getInstance()->isInOpenSim())
            {
                auto preset_name = sky->getName();
                LL_DEBUGS("WindlightCaps") << "Current Sky is " << preset_name << LL_ENDL;
                if (preset_name == "_default_")
                {
                    preset_name = "Default";
                }
                mWLPresetsCombo->selectByValue(preset_name);
            }
#endif
        }
        // Water is not part of legacy day so we need to hunt around
        LLSettingsWater::ptr_t water = LLEnvironment::instance().getEnvironmentFixedWater(LLEnvironment::ENV_LOCAL);
        if (water)
        {
            LLUUID asset_id = water->getAssetId();
            if (asset_id.notNull())
            {
                mWaterPresetsCombo->selectByValue(LLSD(asset_id));
            }
#ifdef OPENSIM
            else if (LLGridManager::getInstance()->isInOpenSim())
            {
                auto preset_name = water->getName();
                if (preset_name == "_default_")
                {
                    preset_name = "Default";
                }
                mWaterPresetsCombo->selectByValue(preset_name);
            }
#endif //OPENSIM
        }
    }
    else
    {
        // LLEnvironment::ENV_REGION:
        // LLEnvironment::ENV_PARCEL:
        mWLPresetsCombo->selectByValue(LLSD(PRESET_NAME_REGION_DEFAULT));
        // mWaterPresetsCombo->selectByValue(LLSD(PRESET_NAME_REGION_DEFAULT));
        // mDayCyclePresetsCombo->selectByValue(LLSD(PRESET_NAME_REGION_DEFAULT));
    }

    setDefaultPresetsEnabled(false);
}

bool APFloaterPhototools::isValidPreset(const LLSD& preset)
{
    if (preset.isUUID())
    {
        if(!preset.asUUID().isNull()){ return true;}
    }
    else if (preset.isString())
    {
        if(!preset.asString().empty() &&
            preset.asString() != PRESET_NAME_REGION_DEFAULT &&
            preset.asString() != PRESET_NAME_DAY_CYCLE &&
            preset.asString() != PRESET_NAME_NONE)
        {
            return true;
        }
    }
    return false;
}

void APFloaterPhototools::stepComboBox(LLComboBox* ctrl, bool forward)
{
    S32 increment = (forward ? 1 : -1);
    S32 lastitem = ctrl->getItemCount() - 1;
    S32 curid = ctrl->getCurrentIndex();
    S32 startid = curid;

    do
    {
        curid += increment;
        if (curid < 0)
        {
            curid = lastitem;
        }
        else if (curid > lastitem)
        {
            curid = 0;
        }
        ctrl->setCurrentByIndex(curid);
    }
    while (!isValidPreset(ctrl->getSelectedValue()) && curid != startid);
}

void APFloaterPhototools::selectSkyPreset(const LLSD& preset)
{
// Opensim continued W/L support
#ifdef OPENSIM
    if(!preset.isUUID() && LLGridManager::getInstance()->isInOpenSim())
    {
        LLSettingsSky::ptr_t legacy_sky = nullptr;
        LLSD messages;

        legacy_sky = LLEnvironment::createSkyFromLegacyPreset(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "windlight", "skies", preset.asString() + ".xml"), messages);

        if (legacy_sky)
        {
            // Need to preserve current sky manually in this case in contrast to asset-based settings
            LLSettingsWater::ptr_t current_water = LLEnvironment::instance().getCurrentWater();
            LLEnvironment::instance().setEnvironment(LLEnvironment::ENV_LOCAL, legacy_sky, current_water);
            LLEnvironment::instance().setSelectedEnvironment(LLEnvironment::ENV_LOCAL);
            LLEnvironment::instance().updateEnvironment(static_cast<LLSettingsBase::Seconds>(gSavedSettings.getF32("FSEnvironmentManualTransitionTime")));
        }
        else
        {
            LL_WARNS() << "Legacy windlight conversion failed for " << preset << " existing env unchanged." << LL_ENDL;
            return;
        }
    }
    else // note the else here bridges the endif
#endif
    {
        LLEnvironment::instance().setSelectedEnvironment(LLEnvironment::ENV_LOCAL);
        LLEnvironment::instance().setManualEnvironment(LLEnvironment::ENV_LOCAL, preset.asUUID());
    }
}

void APFloaterPhototools::selectWaterPreset(const LLSD& preset)
{
#ifdef OPENSIM
    if(!preset.isUUID() && LLGridManager::getInstance()->isInOpenSim())
    {
        LLSettingsWater::ptr_t legacy_water = nullptr;
        LLSD messages;
        legacy_water = LLEnvironment::createWaterFromLegacyPreset(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "windlight", "water", preset.asString() + ".xml"), messages);
        if (legacy_water)
        {
            // Need to preserve current sky manually in this case in contrast to asset-based settings
            LLSettingsSky::ptr_t current_sky = LLEnvironment::instance().getCurrentSky();
            LLEnvironment::instance().setEnvironment(LLEnvironment::ENV_LOCAL, current_sky, legacy_water);
            LLEnvironment::instance().setSelectedEnvironment(LLEnvironment::ENV_LOCAL);
            LLEnvironment::instance().updateEnvironment(static_cast<LLSettingsBase::Seconds>(gSavedSettings.getF32("FSEnvironmentManualTransitionTime")));
        }
        else
        {
            LL_WARNS() << "Legacy windlight conversion failed for " << preset << " existing env unchanged." << LL_ENDL;
            return;
        }
    }
    else // beware the trailing else here.
#endif
    {
        LLEnvironment::instance().setSelectedEnvironment(LLEnvironment::ENV_LOCAL);
        LLEnvironment::instance().setManualEnvironment(LLEnvironment::ENV_LOCAL, preset.asUUID());
    }
}

// void APFloaterPhototools::selectDayCyclePreset(const LLSD& preset)
// {
// #ifdef OPENSIM
    // if(!preset.isUUID() && LLGridManager::getInstance()->isInOpenSim())
    // {
        // LLSettingsDay::ptr_t legacyday = nullptr;
        // LLSD messages;
        // legacyday = LLEnvironment::createDayCycleFromLegacyPreset(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "windlight", "days", preset.asString() + ".xml"), messages);
        // if (legacyday)
        // {
            // LLEnvironment::instance().setEnvironment(LLEnvironment::ENV_LOCAL, legacyday);
            // LLEnvironment::instance().setSelectedEnvironment(LLEnvironment::ENV_LOCAL);
            // LLEnvironment::instance().updateEnvironment(static_cast<LLSettingsBase::Seconds>(gSavedSettings.getF32("FSEnvironmentManualTransitionTime")));
        // }
        // else
        // {
            // LL_WARNS() << "Legacy windlight conversion failed for " << preset << " existing env unchanged." << LL_ENDL;
            // return;
        // }
    // }
    // else // beware trailing else that bridges the endif
// #endif
    // {
        // LLEnvironment::instance().setSelectedEnvironment(LLEnvironment::ENV_LOCAL);
        // LLEnvironment::instance().setManualEnvironment(LLEnvironment::ENV_LOCAL, preset.asUUID());
    // }
// }

void APFloaterPhototools::onChangeWaterPreset()
{
    if (!isValidPreset(mWaterPresetsCombo->getSelectedValue()))
    {
        stepComboBox(mWaterPresetsCombo, true);
    }

    if (isValidPreset(mWaterPresetsCombo->getSelectedValue()))
    {
        selectWaterPreset(mWaterPresetsCombo->getSelectedValue());
    }
    else
    {
        LLNotificationsUtil::add("NoValidEnvSettingFound");
    }
}

void APFloaterPhototools::onChangeSkyPreset()
{
    if (!isValidPreset(mWLPresetsCombo->getSelectedValue()))
    {
        stepComboBox(mWLPresetsCombo, true);
    }

    if (isValidPreset(mWLPresetsCombo->getSelectedValue()))
    {
        selectSkyPreset(mWLPresetsCombo->getSelectedValue());
    }
    else
    {
        LLNotificationsUtil::add("NoValidEnvSettingFound");
    }
}

// void APFloaterPhototools::onChangeDayCyclePreset()
// {
    // if (!isValidPreset(mDayCyclePresetsCombo->getSelectedValue()))
    // {
        // stepComboBox(mDayCyclePresetsCombo, true);
    // }

    // if (isValidPreset(mDayCyclePresetsCombo->getSelectedValue()))
    // {
        // selectDayCyclePreset(mDayCyclePresetsCombo->getSelectedValue());
    // }
    // else
    // {
        // LLNotificationsUtil::add("NoValidEnvSettingFound");
    // }
// }

void APFloaterPhototools::onClickWaterPrev()
{
    stepComboBox(mWaterPresetsCombo, false);
    selectWaterPreset(mWaterPresetsCombo->getSelectedValue());
}

void APFloaterPhototools::onClickWaterNext()
{
    stepComboBox(mWaterPresetsCombo, true);
    selectWaterPreset(mWaterPresetsCombo->getSelectedValue());
}

void APFloaterPhototools::onClickSkyPrev()
{
    stepComboBox(mWLPresetsCombo, false);
    selectSkyPreset(mWLPresetsCombo->getSelectedValue());
}

void APFloaterPhototools::onClickSkyNext()
{
    stepComboBox(mWLPresetsCombo, true);
    selectSkyPreset(mWLPresetsCombo->getSelectedValue());
}

// void APFloaterPhototools::onClickDayCyclePrev()
// {
    // stepComboBox(mDayCyclePresetsCombo, false);
    // selectDayCyclePreset(mDayCyclePresetsCombo->getSelectedValue());
// }

// void APFloaterPhototools::onClickDayCycleNext()
// {
    // stepComboBox(mDayCyclePresetsCombo, true);
    // selectDayCyclePreset(mDayCyclePresetsCombo->getSelectedValue());
// }

void APFloaterPhototools::setSelectedSky(const std::string& preset_name)
{
    mWLPresetsCombo->setValue(LLSD(preset_name));
}

void APFloaterPhototools::setSelectedWater(const std::string& preset_name)
{
    mWaterPresetsCombo->setValue(LLSD(preset_name));
}

// void APFloaterPhototools::setSelectedDayCycle(const std::string& preset_name)
// {
    // mDayCyclePresetsCombo->setValue(LLSD(preset_name));
    // mWLPresetsCombo->setValue(LLSD(PRESET_NAME_DAY_CYCLE));
    // mWaterPresetsCombo->setValue(LLSD(PRESET_NAME_DAY_CYCLE));
// }

// Shadows Tab //

void APFloaterPhototools::refreshSettings()
{
    LLVector3 renderShadowSplitExponent = gSavedSettings.getVector3("RenderShadowSplitExponent");

    mSliderRenderShadowSplitExponentY->setValue(renderShadowSplitExponent.mV[VY]);
    mSpinnerRenderShadowSplitExponentY->setValue(renderShadowSplitExponent.mV[VY]);

    LLVector3 renderRenderShadowGaussian = gSavedSettings.getVector3("RenderShadowGaussian");
     
    mSliderRenderShadowGaussianX->setValue(renderRenderShadowGaussian.mV[VX]);
    mSpinnerRenderShadowGaussianX->setValue(renderRenderShadowGaussian.mV[VX]);

    mSliderRenderShadowGaussianY->setValue(renderRenderShadowGaussian.mV[VY]);
    mSpinnerRenderShadowGaussianY->setValue(renderRenderShadowGaussian.mV[VY]);
    
    LLVector3 renderSSAOEffect = gSavedSettings.getVector3("RenderSSAOEffect");
    
    mSpinnerRenderSSAOEffectX->setValue(renderSSAOEffect.mV[VX]);
    mSliderRenderSSAOEffectX->setValue(renderSSAOEffect.mV[VX]);

    mSpinnerRenderSSAOEffectY->setValue(renderSSAOEffect.mV[VY]);
    mSliderRenderSSAOEffectY->setValue(renderSSAOEffect.mV[VY]);
}

void APFloaterPhototools::onChangeRenderShadowSplitExponentSliderY()
{
    LLVector3 renderShadowSplitExponent = gSavedSettings.getVector3("RenderShadowSplitExponent");
    renderShadowSplitExponent.mV[VY] = mSliderRenderShadowSplitExponentY->getValueF32();
    mSpinnerRenderShadowSplitExponentY->setValue(renderShadowSplitExponent.mV[VY]);
    gSavedSettings.setVector3("RenderShadowSplitExponent", renderShadowSplitExponent);
}

void APFloaterPhototools::onChangeRenderShadowSplitExponentSpinnerY()
{
    LLVector3 renderShadowSplitExponent = gSavedSettings.getVector3("RenderShadowSplitExponent");
    renderShadowSplitExponent.mV[VY] = mSpinnerRenderShadowSplitExponentY->getValueF32();
    mSliderRenderShadowSplitExponentY->setValue(renderShadowSplitExponent.mV[VY]);
    gSavedSettings.setVector3("RenderShadowSplitExponent", renderShadowSplitExponent);
}

void APFloaterPhototools::onClickResetRenderShadowSplitExponentY()
{
    LLVector3 renderShadowSplitExponentDefault = LLVector3(gSavedSettings.getControl("RenderShadowSplitExponent")->getDefault());
    LLVector3 renderShadowSplitExponent = gSavedSettings.getVector3("RenderShadowSplitExponent");
    renderShadowSplitExponent.mV[VY] = renderShadowSplitExponentDefault.mV[VY];
    mSpinnerRenderShadowSplitExponentY->setValue(renderShadowSplitExponent.mV[VY]);
    mSliderRenderShadowSplitExponentY->setValue(renderShadowSplitExponent.mV[VY]);
    gSavedSettings.setVector3("RenderShadowSplitExponent", renderShadowSplitExponent);
}

void APFloaterPhototools::onChangeRenderShadowGaussianSlider()
{
    LLVector3 renderShadowGaussian = gSavedSettings.getVector3("RenderShadowGaussian");
    renderShadowGaussian.mV[VX] = mSliderRenderShadowGaussianX->getValueF32();
    renderShadowGaussian.mV[VY] = mSliderRenderShadowGaussianY->getValueF32();
    mSpinnerRenderShadowGaussianX->setValue(renderShadowGaussian.mV[VX]);
    mSpinnerRenderShadowGaussianY->setValue(renderShadowGaussian.mV[VY]);
    gSavedSettings.setVector3("RenderShadowGaussian", renderShadowGaussian);
}

void APFloaterPhototools::onChangeRenderShadowGaussianSpinner()
{
    LLVector3 renderShadowGaussian = gSavedSettings.getVector3("RenderShadowGaussian");
    renderShadowGaussian.mV[VX] = mSpinnerRenderShadowGaussianX->getValueF32();
    renderShadowGaussian.mV[VY] = mSpinnerRenderShadowGaussianY->getValueF32();
    mSliderRenderShadowGaussianX->setValue(renderShadowGaussian.mV[VX]);
    mSliderRenderShadowGaussianY->setValue(renderShadowGaussian.mV[VY]);
    gSavedSettings.setVector3("RenderShadowGaussian", renderShadowGaussian);
}

void APFloaterPhototools::onClickResetRenderShadowGaussianX()
{
    LLVector3 renderShadowGaussianDefault = LLVector3(gSavedSettings.getControl("RenderShadowGaussian")->getDefault());
    LLVector3 renderShadowGaussian = gSavedSettings.getVector3("RenderShadowGaussian");
    renderShadowGaussian.mV[VX] = renderShadowGaussianDefault.mV[VX];
    mSpinnerRenderShadowGaussianX->setValue(renderShadowGaussian.mV[VX]);
    mSliderRenderShadowGaussianX->setValue(renderShadowGaussian.mV[VX]);
    gSavedSettings.setVector3("RenderShadowGaussian", renderShadowGaussian);
}

void APFloaterPhototools::onClickResetRenderShadowGaussianY()
{
    LLVector3 renderShadowGaussianDefault = LLVector3(gSavedSettings.getControl("RenderShadowGaussian")->getDefault());
    LLVector3 renderShadowGaussian = gSavedSettings.getVector3("RenderShadowGaussian");
    renderShadowGaussian.mV[VY] = renderShadowGaussianDefault.mV[VY];
    mSpinnerRenderShadowGaussianY->setValue(renderShadowGaussian.mV[VY]);
    mSliderRenderShadowGaussianY->setValue(renderShadowGaussian.mV[VY]);
    gSavedSettings.setVector3("RenderShadowGaussian", renderShadowGaussian);
}

void APFloaterPhototools::onChangeRenderSSAOEffectSlider()
{
    LLVector3 renderSSAOEffect = gSavedSettings.getVector3("RenderSSAOEffect");
    renderSSAOEffect.mV[VX] = mSliderRenderSSAOEffectX->getValueF32();
    mSpinnerRenderSSAOEffectX->setValue(renderSSAOEffect.mV[VX]);
    gSavedSettings.setVector3("RenderSSAOEffect", renderSSAOEffect);
}

void APFloaterPhototools::onChangeRenderSSAOEffectSpinner()
{
    LLVector3 renderSSAOEffect = gSavedSettings.getVector3("RenderSSAOEffect");
    renderSSAOEffect.mV[VX] = mSpinnerRenderSSAOEffectX->getValueF32();
    mSliderRenderSSAOEffectX->setValue(renderSSAOEffect.mV[VX]);
    gSavedSettings.setVector3("RenderSSAOEffect", renderSSAOEffect);
}

void APFloaterPhototools::onClickResetRenderSSAOEffectX()
{
    LLVector3 renderSSAOEffectDefault = LLVector3(gSavedSettings.getControl("RenderSSAOEffect")->getDefault());
    LLVector3 renderSSAOEffect = gSavedSettings.getVector3("RenderSSAOEffect");
    renderSSAOEffect.mV[VX] = renderSSAOEffectDefault.mV[VX];
    mSpinnerRenderSSAOEffectX->setValue(renderSSAOEffect.mV[VX]);
    mSliderRenderSSAOEffectX->setValue(renderSSAOEffect.mV[VX]);
    gSavedSettings.setVector3("RenderSSAOEffect", renderSSAOEffect);
}

void APFloaterPhototools::onChangeRenderSSAOEffectSliderY()
{
    LLVector3 renderSSAOEffect = gSavedSettings.getVector3("RenderSSAOEffect");
    renderSSAOEffect.mV[VY] = mSliderRenderSSAOEffectY->getValueF32();
    mSpinnerRenderSSAOEffectY->setValue(renderSSAOEffect.mV[VY]);
    gSavedSettings.setVector3("RenderSSAOEffect", renderSSAOEffect);
}

void APFloaterPhototools::onChangeRenderSSAOEffectSpinnerY()
{
    LLVector3 renderSSAOEffect = gSavedSettings.getVector3("RenderSSAOEffect");
    renderSSAOEffect.mV[VY] = mSpinnerRenderSSAOEffectY->getValueF32();
    mSliderRenderSSAOEffectY->setValue(renderSSAOEffect.mV[VY]);
    gSavedSettings.setVector3("RenderSSAOEffect", renderSSAOEffect);
}

void APFloaterPhototools::onClickResetRenderSSAOEffectY()
{
    LLVector3 renderSSAOEffectDefault = LLVector3(gSavedSettings.getControl("RenderSSAOEffect")->getDefault());
    LLVector3 renderSSAOEffect = gSavedSettings.getVector3("RenderSSAOEffect");
    renderSSAOEffect.mV[VY] = renderSSAOEffectDefault.mV[VY];
    mSpinnerRenderSSAOEffectY->setValue(renderSSAOEffect.mV[VY]);
    mSliderRenderSSAOEffectY->setValue(renderSSAOEffect.mV[VY]);
    gSavedSettings.setVector3("RenderSSAOEffect", renderSSAOEffect);
}

// General Tab //

void APFloaterPhototools::updateMaxNonImpostors(const LLSD& newvalue)
{
    // Called when the IndirectMaxNonImpostors control changes
    // Responsible for fixing the setting RenderAvatarMaxNonImpostors
    U32 value = newvalue.asInteger();

    if (0 == value || LLVOAvatar::NON_IMPOSTORS_MAX_SLIDER <= value)
    {
        value=0;
    }
    gSavedSettings.setU32("RenderAvatarMaxNonImpostors", value);
    LLVOAvatar::updateImpostorRendering(value); // make it effective immediately
}

void APFloaterPhototools::updateMaxComplexity()
{
    // Called when the IndirectMaxComplexity control changes
    LLAvatarComplexityControls::updateMax(mMaxComplexitySlider, mMaxComplexityLabel);
}

void APFloaterPhototools::updateMaxComplexityLabel(const LLSD& newvalue)
{
    U32 value = newvalue.asInteger();

    LLAvatarComplexityControls::setText(value, mMaxComplexityLabel);
}

// Atmospheric Colors 

void APFloaterPhototools::onAmbientLightChanged()
{
    LLSettingsSky::ptr_t sky = LLEnvironment::instance().getCurrentSky();
    if (!sky)
        return;
    sky->setAmbientColor(LLColor3(getChild<LLColorSwatchCtrl>(FIELD_SKY_AMBIENT_LIGHT)->get() * SLIDER_SCALE_SUN_AMBIENT));
    sky->update();
}

void APFloaterPhototools::onBlueHorizonChanged()
{
    LLSettingsSky::ptr_t sky = LLEnvironment::instance().getCurrentSky();
    if (!sky)
        return;
    sky->setBlueHorizon(LLColor3(getChild<LLColorSwatchCtrl>(FIELD_SKY_BLUE_HORIZON)->get() * SLIDER_SCALE_BLUE_HORIZON_DENSITY));
    sky->update();
}

void APFloaterPhototools::onBlueDensityChanged()
{
    LLSettingsSky::ptr_t sky = LLEnvironment::instance().getCurrentSky();
    if (!sky)
        return;
    sky->setBlueDensity(LLColor3(getChild<LLColorSwatchCtrl>(FIELD_SKY_BLUE_DENSITY)->get() * SLIDER_SCALE_BLUE_HORIZON_DENSITY));
    sky->update();
}

// Atmosphere Settings

void APFloaterPhototools::onHazeHorizonChanged()
{
    LLSettingsSky::ptr_t sky = LLEnvironment::instance().getCurrentSky();
    if (!sky)
        return;
    sky->setHazeHorizon((F32)getChild<LLUICtrl>(FIELD_SKY_HAZE_HORIZON)->getValue().asReal());
    sky->update();
}

void APFloaterPhototools::onHazeDensityChanged()
{
    LLSettingsSky::ptr_t sky = LLEnvironment::instance().getCurrentSky();
    if (!sky)
        return;
    sky->setHazeDensity((F32)getChild<LLUICtrl>(FIELD_SKY_HAZE_DENSITY)->getValue().asReal());
    sky->update();
}

void APFloaterPhototools::onDensityMultipChanged()
{
    LLSettingsSky::ptr_t sky = LLEnvironment::instance().getCurrentSky();
    if (!sky)
        return;
    F32 density_mult = (F32)getChild<LLUICtrl>(FIELD_SKY_DENSITY_MULTIP)->getValue().asReal();
    density_mult *= SLIDER_SCALE_DENSITY_MULTIPLIER;
    sky->setDensityMultiplier(density_mult);
    sky->update();

}

void APFloaterPhototools::onDistanceMultipChanged()
{
    LLSettingsSky::ptr_t sky = LLEnvironment::instance().getCurrentSky();
    if (!sky)
        return;
    sky->setDistanceMultiplier((F32)getChild<LLUICtrl>(FIELD_SKY_DISTANCE_MULTIP)->getValue().asReal());
    sky->update();

}

void APFloaterPhototools::onMaxAltChanged()
{
    LLSettingsSky::ptr_t sky = LLEnvironment::instance().getCurrentSky();
    if (!sky)
        return;
    sky->setMaxY((F32)getChild<LLUICtrl>(FIELD_SKY_MAX_ALT)->getValue().asReal());
    sky->update();

}

void APFloaterPhototools::onSceneGammaChanged()
{
    LLSettingsSky::ptr_t sky = LLEnvironment::instance().getCurrentSky();
    if (!sky)
        return;
    sky->setGamma((F32)getChild<LLUICtrl>(FIELD_SKY_SCENE_GAMMA)->getValue().asReal());
    sky->update();
}

void APFloaterPhototools::onReflectionProbeAmbianceChanged()
{
    LLSettingsSky::ptr_t sky = LLEnvironment::instance().getCurrentSky();
    if (!sky)
        return;
    F32 ambiance = (F32)getChild<LLUICtrl>(FIELD_REFLECTION_PROBE_AMBIANCE)->getValue().asReal();
    sky->setReflectionProbeAmbiance(ambiance);

    // updateGammaLabel(sky);
    sky->update();
}

void APFloaterPhototools::updateGammaLabel(LLSettingsSky::ptr_t sky) 
{
    if (!sky)
        return;
    static LLCachedControl<bool> should_auto_adjust(gSavedSettings, "RenderSkyAutoAdjustLegacy", false);
    F32 ambiance = sky->getReflectionProbeAmbiance(should_auto_adjust);
    if (ambiance != 0.f)
    {
        childSetValue("scene_gamma_label", LLSD(getString("hdr_string")));
        getChild<LLUICtrl>(FIELD_SKY_SCENE_GAMMA)->setToolTip(getString("hdr_tooltip"));
    }
    else
    {
        childSetValue("scene_gamma_label", LLSD(getString("brightness_string")));
        getChild<LLUICtrl>(FIELD_SKY_SCENE_GAMMA)->setToolTip(std::string());
    }
}

// Rainbow and Halo Settings

void APFloaterPhototools::onMoistureLevelChanged()
{
    LLSettingsSky::ptr_t sky = LLEnvironment::instance().getCurrentSky();
    if (!sky)
        return;
    F32 moisture_level = (F32)getChild<LLUICtrl>(FIELD_SKY_DENSITY_MOISTURE_LEVEL)->getValue().asReal();
    sky->setSkyMoistureLevel(moisture_level);
    sky->update();

}

void APFloaterPhototools::onDropletRadiusChanged()
{
    LLSettingsSky::ptr_t sky = LLEnvironment::instance().getCurrentSky();
    if (!sky)
        return;
    F32 droplet_radius = (F32)getChild<LLUICtrl>(FIELD_SKY_DENSITY_DROPLET_RADIUS)->getValue().asReal();
    sky->setSkyDropletRadius(droplet_radius);
    sky->update();

}

void APFloaterPhototools::onIceLevelChanged()
{
    LLSettingsSky::ptr_t sky = LLEnvironment::instance().getCurrentSky();
    if (!sky)
        return;
    F32 ice_level = (F32)getChild<LLUICtrl>(FIELD_SKY_DENSITY_ICE_LEVEL)->getValue().asReal();
    sky->setSkyIceLevel(ice_level);
    sky->update();

}

// Clouds

void APFloaterPhototools::onCloudColorChanged()
{
    LLSettingsSky::ptr_t sky = LLEnvironment::instance().getCurrentSky();
    if (!sky)
        return;
    sky->setCloudColor(LLColor3(getChild<LLColorSwatchCtrl>(FIELD_SKY_CLOUD_COLOR)->get()));
    sky->update();
}

void APFloaterPhototools::onCloudCoverageChanged()
{
    LLSettingsSky::ptr_t sky = LLEnvironment::instance().getCurrentSky();
    if (!sky)
        return;
    sky->setCloudShadow((F32)getChild<LLUICtrl>(FIELD_SKY_CLOUD_COVERAGE)->getValue().asReal());
    sky->update();
}

void APFloaterPhototools::onCloudScaleChanged()
{
    LLSettingsSky::ptr_t sky = LLEnvironment::instance().getCurrentSky();
    if (!sky)
        return;
    sky->setCloudScale((F32)getChild<LLUICtrl>(FIELD_SKY_CLOUD_SCALE)->getValue().asReal());
    sky->update();
}

void APFloaterPhototools::onCloudVarianceChanged()
{
    LLSettingsSky::ptr_t sky = LLEnvironment::instance().getCurrentSky();
    if (!sky)
        return;
    sky->setCloudVariance((F32)getChild<LLUICtrl>(FIELD_SKY_CLOUD_VARIANCE)->getValue().asReal());
}

void APFloaterPhototools::onCloudDensityChanged()
{
    LLSettingsSky::ptr_t sky = LLEnvironment::instance().getCurrentSky();
    if (!sky)
        return;
    LLColor3 density((F32)getChild<LLUICtrl>(FIELD_SKY_CLOUD_DENSITY_X)->getValue().asReal(),
        (F32)getChild<LLUICtrl>(FIELD_SKY_CLOUD_DENSITY_Y)->getValue().asReal(),
        (F32)getChild<LLUICtrl>(FIELD_SKY_CLOUD_DENSITY_D)->getValue().asReal());

    sky->setCloudPosDensity1(density);
}

void APFloaterPhototools::onCloudDetailChanged()
{
    LLSettingsSky::ptr_t sky = LLEnvironment::instance().getCurrentSky();
    if (!sky)
        return;
    LLColor3 detail((F32)getChild<LLUICtrl>(FIELD_SKY_CLOUD_DETAIL_X)->getValue().asReal(),
        (F32)getChild<LLUICtrl>(FIELD_SKY_CLOUD_DETAIL_Y)->getValue().asReal(),
        (F32)getChild<LLUICtrl>(FIELD_SKY_CLOUD_DETAIL_D)->getValue().asReal());

    sky->setCloudPosDensity2(detail);
}

void APFloaterPhototools::onCloudScrollChanged()
{
    LLSettingsSky::ptr_t sky = LLEnvironment::instance().getCurrentSky();
    if (!sky)
        return;
    LLVector2 scroll(getChild<LLUICtrl>(FIELD_SKY_CLOUD_SCROLL_XY)->getValue());
    sky->setCloudScrollRate(scroll);
}

// void APFloaterPhototools::onCloudMapChanged()
// {
    // LLSettingsSky::ptr_t sky = LLEnvironment::instance().getCurrentSky();
    // if (!sky)
        // return;
    // LLTextureCtrl* ctrl = getChild<LLTextureCtrl>(FIELD_SKY_CLOUD_MAP);
    // sky->setCloudNoiseTextureId(ctrl->getValue().asUUID());
    // sky->update(); 
// }

void APFloaterPhototools::onCloudMapChanged()
{
    LLEnvironment::instance().setSelectedEnvironment(LLEnvironment::ENV_LOCAL);

    LLSettingsSky::ptr_t current_sky = LLEnvironment::instance().getCurrentSky();
    if (!current_sky)
    {
        return;
    }

    LLTextureCtrl* picker_ctrl = getChild<LLTextureCtrl>(FIELD_SKY_CLOUD_MAP);
    if (!picker_ctrl)
    {
         return;
    }

    LLUUID new_texture_id = picker_ctrl->getValue().asUUID();
    LLSettingsSky::ptr_t sky_to_set = current_sky->buildClone();
    sky_to_set->setCloudNoiseTextureId(new_texture_id);

    LLEnvironment::instance().setEnvironment(LLEnvironment::ENV_LOCAL, sky_to_set);
    LLEnvironment::instance().updateEnvironment(LLEnvironment::TRANSITION_INSTANT, true);
    picker_ctrl->setValue(new_texture_id);
}

// Sun and Moon Colors

void APFloaterPhototools::onSunColorChanged()
{
    LLSettingsSky::ptr_t sky = LLEnvironment::instance().getCurrentSky();
    if (!sky)
        return;
    LLColor3 color(getChild<LLColorSwatchCtrl>(FIELD_SKY_SUN_COLOR)->get());

    color *= SLIDER_SCALE_SUN_AMBIENT;

    sky->setSunlightColor(color);
    sky->update();
}

// Sun and Stars Settings

void APFloaterPhototools::onSunImageChanged()
{
    LLSettingsSky::ptr_t sky = LLEnvironment::instance().getCurrentSky();
    if (!sky)
        return;
    sky->setSunTextureId(getChild<LLTextureCtrl>(FIELD_SKY_SUN_IMAGE)->getValue().asUUID());
    sky->update();
}

void APFloaterPhototools::onGlowChanged()
{
    LLSettingsSky::ptr_t sky = LLEnvironment::instance().getCurrentSky();
    if (!sky)
        return;
    LLColor3 glow((F32)getChild<LLUICtrl>(FIELD_SKY_GLOW_SIZE)->getValue().asReal(), 0.0f, (F32)getChild<LLUICtrl>(FIELD_SKY_GLOW_FOCUS)->getValue().asReal());

    // takes 0 - 1.99 UI range -> 40 -> 0.2 range
    glow.mV[0] = (2.0f - glow.mV[0]) * SLIDER_SCALE_GLOW_R;
    glow.mV[2] *= SLIDER_SCALE_GLOW_B;

    sky->setGlow(glow);
    sky->update();
}

void APFloaterPhototools::onStarBrightnessChanged()
{
    LLSettingsSky::ptr_t sky = LLEnvironment::instance().getCurrentSky();
    if (!sky)
        return;
    sky->setStarBrightness((F32)getChild<LLUICtrl>(FIELD_SKY_STAR_BRIGHTNESS)->getValue().asReal());
    sky->update();
}

void APFloaterPhototools::onSunScaleChanged()
{
    LLSettingsSky::ptr_t sky = LLEnvironment::instance().getCurrentSky();
    if (!sky)
        return;
    sky->setSunScale((F32)(getChild<LLUICtrl>(FIELD_SKY_SUN_SCALE)->getValue().asReal()));
    sky->update();
}

void APFloaterPhototools::onSunAzimElevChanged()
{
    F32 azimuth = (F32)getChild<LLUICtrl>(FIELD_SKY_SUN_AZIMUTH)->getValue().asReal();
    F32 elevation = (F32)getChild<LLUICtrl>(FIELD_SKY_SUN_ELEVATION)->getValue().asReal();

    LLQuaternion quat;

    azimuth *= DEG_TO_RAD;
    elevation *= DEG_TO_RAD;

    if (is_approx_zero(elevation))
    {
        elevation = F_APPROXIMATELY_ZERO;
    }

     quat.setAngleAxis(-elevation, 0, 1, 0);
    LLQuaternion az_quat;
    az_quat.setAngleAxis(F_TWO_PI - azimuth, 0, 0, 1);
    quat *= az_quat;


    LLSettingsSky::ptr_t sky = LLEnvironment::instance().getCurrentSky();
    if (sky)
    {
        sky->setSunRotation(quat);
        sky->update();
    }
}

// Moon Settings

void APFloaterPhototools::onMoonImageChanged()
{
    LLSettingsSky::ptr_t sky = LLEnvironment::instance().getCurrentSky();
    if (!sky)
        return;
    sky->setMoonTextureId(getChild<LLTextureCtrl>(FIELD_SKY_MOON_IMAGE)->getValue().asUUID());
    sky->update();
}

void APFloaterPhototools::onMoonScaleChanged()
{
    LLSettingsSky::ptr_t sky = LLEnvironment::instance().getCurrentSky();
    if (!sky)
        return;
    sky->setMoonScale((F32)(getChild<LLUICtrl>(FIELD_SKY_MOON_SCALE)->getValue().asReal()));
    sky->update();
}

void APFloaterPhototools::onMoonBrightnessChanged()
{
    LLSettingsSky::ptr_t sky = LLEnvironment::instance().getCurrentSky();
    if (!sky)
        return;
    sky->setMoonBrightness((F32)(getChild<LLUICtrl>(FIELD_SKY_MOON_BRIGHTNESS)->getValue().asReal()));
    sky->update();
}

void APFloaterPhototools::onMoonAzimElevChanged()
{
    F32 azimuth = (F32)getChild<LLUICtrl>(FIELD_SKY_MOON_AZIMUTH)->getValue().asReal();
    F32 elevation = (F32)getChild<LLUICtrl>(FIELD_SKY_MOON_ELEVATION)->getValue().asReal();
    LLQuaternion quat;

    azimuth *= DEG_TO_RAD;
    elevation *= DEG_TO_RAD;

    if (is_approx_zero(elevation))
    {
        elevation = F_APPROXIMATELY_ZERO;
    }

    quat.setAngleAxis(-elevation, 0, 1, 0);
    LLQuaternion az_quat;
    az_quat.setAngleAxis(F_TWO_PI - azimuth, 0, 0, 1);
    quat *= az_quat;

    // getChild<LLVirtualTrackball>(FIELD_SKY_MOON_ROTATION)->setRotation(quat);

    LLSettingsSky::ptr_t sky = LLEnvironment::instance().getCurrentSky();
    if (sky)
    {
        sky->setMoonRotation(quat);
        sky->update();
    }
}

// Atmosphere General Functions

void APFloaterPhototools::refreshSky()
{

    LLSettingsSky::ptr_t sky = LLEnvironment::instance().getCurrentSky();

    if (!sky)
    {
        setAllChildrenEnabled(false);
        return;
    }

    setEnabled(true);
    setAllChildrenEnabled(true);

    // Atmospheric Colors
    getChild<LLColorSwatchCtrl>(FIELD_SKY_AMBIENT_LIGHT)->set(sky->getAmbientColor() / SLIDER_SCALE_SUN_AMBIENT);
    getChild<LLColorSwatchCtrl>(FIELD_SKY_BLUE_HORIZON)->set(sky->getBlueHorizon() / SLIDER_SCALE_BLUE_HORIZON_DENSITY);
    getChild<LLColorSwatchCtrl>(FIELD_SKY_BLUE_DENSITY)->set(sky->getBlueDensity() / SLIDER_SCALE_BLUE_HORIZON_DENSITY);
    
    // Atmosphere Settings
    getChild<LLUICtrl>(FIELD_SKY_HAZE_HORIZON)->setValue(sky->getHazeHorizon());
    getChild<LLUICtrl>(FIELD_SKY_HAZE_DENSITY)->setValue(sky->getHazeDensity());
    
    F32 density_mult = sky->getDensityMultiplier();
    density_mult /= SLIDER_SCALE_DENSITY_MULTIPLIER;
    
    getChild<LLUICtrl>(FIELD_SKY_DENSITY_MULTIP)->setValue(density_mult);
    getChild<LLUICtrl>(FIELD_SKY_DISTANCE_MULTIP)->setValue(sky->getDistanceMultiplier());
    getChild<LLUICtrl>(FIELD_SKY_MAX_ALT)->setValue(sky->getMaxY());
    
    getChild<LLUICtrl>(FIELD_SKY_SCENE_GAMMA)->setValue(sky->getGamma());
    static LLCachedControl<bool> should_auto_adjust(gSavedSettings, "RenderSkyAutoAdjustLegacy", false);
    
    F32 rp_ambiance     = sky->getReflectionProbeAmbiance(should_auto_adjust);
    getChild<LLUICtrl>(FIELD_REFLECTION_PROBE_AMBIANCE)->setValue(rp_ambiance);
    
    // updateGammaLabel(sky);
    
    // Rainbow and Halo Settings
    F32 moisture_level  = sky->getSkyMoistureLevel();
    getChild<LLUICtrl>(FIELD_SKY_DENSITY_MOISTURE_LEVEL)->setValue(moisture_level);
        
    F32 droplet_radius  = sky->getSkyDropletRadius();
    getChild<LLUICtrl>(FIELD_SKY_DENSITY_DROPLET_RADIUS)->setValue(droplet_radius);
    
    F32 ice_level       = sky->getSkyIceLevel();
    getChild<LLUICtrl>(FIELD_SKY_DENSITY_ICE_LEVEL)->setValue(ice_level);
    
    
    // Cloud Settings
    getChild<LLColorSwatchCtrl>(FIELD_SKY_CLOUD_COLOR)->set(sky->getCloudColor());
    getChild<LLUICtrl>(FIELD_SKY_CLOUD_COVERAGE)->setValue(sky->getCloudShadow());
    getChild<LLUICtrl>(FIELD_SKY_CLOUD_SCALE)->setValue(sky->getCloudScale());
    getChild<LLUICtrl>(FIELD_SKY_CLOUD_VARIANCE)->setValue(sky->getCloudVariance());
    
    LLVector3 cloudDensity(sky->getCloudPosDensity1().getValue());
    getChild<LLUICtrl>(FIELD_SKY_CLOUD_DENSITY_X)->setValue(cloudDensity[0]);
    getChild<LLUICtrl>(FIELD_SKY_CLOUD_DENSITY_Y)->setValue(cloudDensity[1]);
    getChild<LLUICtrl>(FIELD_SKY_CLOUD_DENSITY_D)->setValue(cloudDensity[2]);
    
    LLVector3 cloudDetail(sky->getCloudPosDensity2().getValue());
    getChild<LLUICtrl>(FIELD_SKY_CLOUD_DETAIL_X)->setValue(cloudDetail[0]);
    getChild<LLUICtrl>(FIELD_SKY_CLOUD_DETAIL_Y)->setValue(cloudDetail[1]);
    getChild<LLUICtrl>(FIELD_SKY_CLOUD_DETAIL_D)->setValue(cloudDetail[2]);
    
    LLVector2 cloudScroll(sky->getCloudScrollRate());
    getChild<LLUICtrl>(FIELD_SKY_CLOUD_SCROLL_XY)->setValue(cloudScroll.getValue());

    getChild<LLTextureCtrl>(FIELD_SKY_CLOUD_MAP)->setValue(sky->getCloudNoiseTextureId());


    // Sun and Moon Colors
    getChild<LLColorSwatchCtrl>(FIELD_SKY_SUN_COLOR)->set(sky->getSunlightColor() / SLIDER_SCALE_SUN_AMBIENT);

    // Sun and Stars Settings
    getChild<LLTextureCtrl>(FIELD_SKY_SUN_IMAGE)->setValue(sky->getSunTextureId());
    
    LLColor3 glow(sky->getGlow());

        // takes 40 - 0.2 range -> 0 - 1.99 UI range
    getChild<LLUICtrl>(FIELD_SKY_GLOW_SIZE)->setValue(2.0 - (glow.mV[0] / SLIDER_SCALE_GLOW_R));
    getChild<LLUICtrl>(FIELD_SKY_GLOW_FOCUS)->setValue(glow.mV[2] / SLIDER_SCALE_GLOW_B);
    getChild<LLUICtrl>(FIELD_SKY_STAR_BRIGHTNESS)->setValue(sky->getStarBrightness());
    getChild<LLUICtrl>(FIELD_SKY_SUN_SCALE)->setValue(sky->getSunScale());

        // Sun rotation
    LLQuaternion quat = sky->getSunRotation();
    F32 azimuth;
    F32 elevation;
    LLVirtualTrackball::getAzimuthAndElevationDeg(quat, azimuth, elevation);

    getChild<LLUICtrl>(FIELD_SKY_SUN_AZIMUTH)->setValue(azimuth);
    getChild<LLUICtrl>(FIELD_SKY_SUN_ELEVATION)->setValue(elevation);
    

    // Moon Settings
    
    getChild<LLTextureCtrl>(FIELD_SKY_MOON_IMAGE)->setValue(sky->getMoonTextureId());
    getChild<LLUICtrl>(FIELD_SKY_MOON_SCALE)->setValue(sky->getMoonScale());
    getChild<LLUICtrl>(FIELD_SKY_MOON_BRIGHTNESS)->setValue(sky->getMoonBrightness());
    
    
        // Moon rotation
    quat = sky->getMoonRotation();
    LLVirtualTrackball::getAzimuthAndElevationDeg(quat, azimuth, elevation);

    getChild<LLUICtrl>(FIELD_SKY_MOON_AZIMUTH)->setValue(azimuth);
    getChild<LLUICtrl>(FIELD_SKY_MOON_ELEVATION)->setValue(elevation);


    // getChild<LLTextureCtrl>(FIELD_WATER_NORMAL_MAP)->setValue(mLiveWater->getNormalMapID());
    //getChild<LLVirtualTrackball>(FIELD_SKY_SUN_ROTATION)->setRotation(quat);
    // getChild<LLVirtualTrackball>(FIELD_SKY_MOON_ROTATION)->setRotation(quat);
}
