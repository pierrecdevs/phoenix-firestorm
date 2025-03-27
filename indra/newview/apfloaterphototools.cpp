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
    
    // Make sure IndirectMaxNonImpostors gets set properly
    LLAvatarComplexityControls::setIndirectMaxNonImpostors();

    // Make sure IndirectMaxComplexity gets set properly
    LLAvatarComplexityControls::setIndirectMaxArc();
    LLAvatarComplexityControls::setText(gSavedSettings.getU32("RenderAvatarMaxComplexity"), mMaxComplexityLabel);

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
    
    getChild<LLUICtrl>("DCPresetsCombo")->setCommitCallback(boost::bind(&APFloaterPhototools::onChangeDayCyclePreset, this));   
    getChild<LLUICtrl>("DCPrevPreset")->setCommitCallback(boost::bind(&APFloaterPhototools::onClickDayCyclePrev, this));
    getChild<LLUICtrl>("DCNextPreset")->setCommitCallback(boost::bind(&APFloaterPhototools::onClickDayCycleNext, this));

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

    mEnvChangedConnection = LLEnvironment::instance().setEnvironmentChanged([this](LLEnvironment::EnvSelection_t env, S32 version){ setSelectedEnvironment(); });

    mMaxComplexitySlider->setCommitCallback(boost::bind(&APFloaterPhototools::updateMaxComplexity, this));
    gSavedSettings.getControl("RenderAvatarMaxComplexity")->getCommitSignal()->connect(boost::bind(&APFloaterPhototools::updateMaxComplexityLabel, this, _2));
    gSavedSettings.getControl("IndirectMaxNonImpostors")->getCommitSignal()->connect(boost::bind(&APFloaterPhototools::updateMaxNonImpostors, this, _2));
    
}

bool APFloaterPhototools::postBuild()
{
    mShowMySettingsCheckbox = getChild<LLCheckBoxCtrl>("show_my_settings_checkbox"); 

    mWaterPresetsCombo = getChild<LLComboBox>("WaterPresetsCombo");
    mWLPresetsCombo = getChild<LLComboBox>("WLPresetsCombo");
    mDayCyclePresetsCombo = getChild<LLComboBox>("DCPresetsCombo");
 
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

    refreshSettings(); 

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
    std::multimap<std::string, LLUUID> daycycle_map;

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
                    daycycle_map.insert(std::make_pair(item->getName(), item->getAssetUUID()));
                    break;
                default:
                    LL_WARNS() << "Found invalid setting: " << item->getName() << LL_ENDL;
                    break;
            }
        }
    }

    loadWaterPresets(water_map);
    loadSkyPresets(sky_map);
    loadDayCyclePresets(daycycle_map);
}

void APFloaterPhototools::loadDayCyclePresets(const std::multimap<std::string, LLUUID>& daycycle_map)
{
    mDayCyclePresetsCombo->operateOnAll(LLComboBox::OP_DELETE);
    mDayCyclePresetsCombo->add(LLTrans::getString("QP_WL_Region_Default"), LLSD(PRESET_NAME_REGION_DEFAULT), EAddPosition::ADD_BOTTOM, false);
    mDayCyclePresetsCombo->add(LLTrans::getString("QP_WL_None"), LLSD(PRESET_NAME_NONE), EAddPosition::ADD_BOTTOM, false);
    mDayCyclePresetsCombo->addSeparator();

    // Add setting presets.
    for (std::multimap<std::string, LLUUID>::const_iterator it = daycycle_map.begin(); it != daycycle_map.end(); ++it)
    {
        const std::string& preset_name = (*it).first;
        const LLUUID& asset_id = (*it).second;

        if (!preset_name.empty())
        {
            mDayCyclePresetsCombo->add(preset_name, LLSD(asset_id));
        }
    }
// <FS:Beq> Opensim legacy windlight support
// Opensim may support both environment and extenvironment caps on the same region
// we also need these disabled in SL on the OpenSim build.
#ifdef OPENSIM
    if(LLGridManager::getInstance()->isInOpenSim())
    {
        LL_DEBUGS("WindlightCaps") << "Adding legacy day cycle presets to QP" << LL_ENDL;
        // WL still supported
        if (!daycycle_map.empty() && !LLEnvironment::getInstance()->mLegacyDayCycles.empty())
        {
            mDayCyclePresetsCombo->addSeparator();
        }
        for(const auto& preset_name : LLEnvironment::getInstance()->mLegacyDayCycles)
        {
            // we add by name and only build the envp on demand
            LL_DEBUGS("WindlightCaps") << "Adding legacy day cycle " << preset_name << LL_ENDL;
            mDayCyclePresetsCombo->add(preset_name, LLSD(preset_name));
        }
        LL_DEBUGS("WindlightCaps") << "Done: Adding legacy day cycle presets to QP" << LL_ENDL;
    }
#endif
}

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

    item = mDayCyclePresetsCombo->getItemByValue(LLSD(PRESET_NAME_REGION_DEFAULT));
    if (item) item->setEnabled(enabled);

    item = mDayCyclePresetsCombo->getItemByValue(LLSD(PRESET_NAME_NONE));
    if (item) item->setEnabled(enabled);
}

void APFloaterPhototools::setSelectedEnvironment()
{
    //LL_INFOS() << "EEP: getSelectedEnvironment: " << LLEnvironment::instance().getSelectedEnvironment() << LL_ENDL;

    setDefaultPresetsEnabled(true);
    mWLPresetsCombo->selectByValue(LLSD(PRESET_NAME_REGION_DEFAULT));
    mWaterPresetsCombo->selectByValue(LLSD(PRESET_NAME_REGION_DEFAULT));
    mDayCyclePresetsCombo->selectByValue(LLSD(PRESET_NAME_REGION_DEFAULT));

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
                mDayCyclePresetsCombo->selectByValue(LLSD(day->getAssetId()));
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
                mDayCyclePresetsCombo->selectByValue(preset_name);
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
            mDayCyclePresetsCombo->selectByValue(LLSD(PRESET_NAME_NONE));
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
        mWaterPresetsCombo->selectByValue(LLSD(PRESET_NAME_REGION_DEFAULT));
        mDayCyclePresetsCombo->selectByValue(LLSD(PRESET_NAME_REGION_DEFAULT));
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

void APFloaterPhototools::selectDayCyclePreset(const LLSD& preset)
{
#ifdef OPENSIM
    if(!preset.isUUID() && LLGridManager::getInstance()->isInOpenSim())
    {
        LLSettingsDay::ptr_t legacyday = nullptr;
        LLSD messages;
        legacyday = LLEnvironment::createDayCycleFromLegacyPreset(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "windlight", "days", preset.asString() + ".xml"), messages);
        if (legacyday)
        {
            LLEnvironment::instance().setEnvironment(LLEnvironment::ENV_LOCAL, legacyday);
            LLEnvironment::instance().setSelectedEnvironment(LLEnvironment::ENV_LOCAL);
            LLEnvironment::instance().updateEnvironment(static_cast<LLSettingsBase::Seconds>(gSavedSettings.getF32("FSEnvironmentManualTransitionTime")));
        }
        else
        {
            LL_WARNS() << "Legacy windlight conversion failed for " << preset << " existing env unchanged." << LL_ENDL;
            return;
        }
    }
    else // beware trailing else that bridges the endif
#endif
    {
        LLEnvironment::instance().setSelectedEnvironment(LLEnvironment::ENV_LOCAL);
        LLEnvironment::instance().setManualEnvironment(LLEnvironment::ENV_LOCAL, preset.asUUID());
    }
}

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

void APFloaterPhototools::onChangeDayCyclePreset()
{
    if (!isValidPreset(mDayCyclePresetsCombo->getSelectedValue()))
    {
        stepComboBox(mDayCyclePresetsCombo, true);
    }

    if (isValidPreset(mDayCyclePresetsCombo->getSelectedValue()))
    {
        selectDayCyclePreset(mDayCyclePresetsCombo->getSelectedValue());
    }
    else
    {
        LLNotificationsUtil::add("NoValidEnvSettingFound");
    }
}

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

void APFloaterPhototools::onClickDayCyclePrev()
{
    stepComboBox(mDayCyclePresetsCombo, false);
    selectDayCyclePreset(mDayCyclePresetsCombo->getSelectedValue());
}

void APFloaterPhototools::onClickDayCycleNext()
{
    stepComboBox(mDayCyclePresetsCombo, true);
    selectDayCyclePreset(mDayCyclePresetsCombo->getSelectedValue());
}

void APFloaterPhototools::setSelectedSky(const std::string& preset_name)
{
    mWLPresetsCombo->setValue(LLSD(preset_name));
}

void APFloaterPhototools::setSelectedWater(const std::string& preset_name)
{
    mWaterPresetsCombo->setValue(LLSD(preset_name));
}

void APFloaterPhototools::setSelectedDayCycle(const std::string& preset_name)
{
    mDayCyclePresetsCombo->setValue(LLSD(preset_name));
    mWLPresetsCombo->setValue(LLSD(PRESET_NAME_DAY_CYCLE));
    mWaterPresetsCombo->setValue(LLSD(PRESET_NAME_DAY_CYCLE));
}

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