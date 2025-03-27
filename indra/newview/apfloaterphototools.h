/**
 * @file apfloaterphototools.h
 * @brief Phototools panel
 *
 * $LicenseInfo:firstyear=2011&license=viewerlgpl$
 * Copyright (C) 2025, William Weaver (paperwork) @ Second Life
 * Copyright (C) 2011, WoLf Loonie @ Second Life
 * Copyright (C) 2013, Zi Ree @ Second Life
 * Copyright (C) 2013, Ansariel Hiller @ Second Life
 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
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

// Header Guard - Prevents the compiler from using this header more than once
#ifndef PHOTOTOOLS_H
#define PHOTOTOOLS_H

// Iclude Directives - tells the compiler other headers that this header needs 
#include "llenvironment.h"
#include "llfloater.h"

// Forward Declarations - declare specific classes that are needed in order to build this header and cpp without have to create a dependnacy on additonal headers to reduce complile time and complexity
class LLCheckBoxCtrl;
class LLComboBox;
class LLLayoutPanel;
class LLLayoutStack;
class LLLineEditor;
class LLSlider;
class LLSliderCtrl;
class LLSpinCtrl;
class LLTextBox;
class LLColorSwatchCtrl;

// defines something I don't know what or whie - Find this the most confusion thing here - don't get this at all
#define PRESET_NAME_REGION_DEFAULT "__Regiondefault__"
#define PRESET_NAME_DAY_CYCLE "__Day_Cycle__"
#define PRESET_NAME_NONE "__None__"

// this is type defnioaiton, it defines a type of thing, here phototools update param, but I don't understand it and have no idea how it link to the cpp and why
typedef enum e_phototools_update_param
{
    PT_PARAM_SKY,
    PT_PARAM_WATER,
    PT_PARAM_DAYCYCLE
} EPhototoolsParam;

// Class Definition - I understand this defines this class, which makes sense because this the header file for this floater and it is WHERE we create the conept of the FloaterPhotools, so this is where we keep our blue print. The public LLfloater least os know the templaye we are making this blueprint from
class APFloaterPhototools : public LLFloater
{
    friend class LLFloaterReg;

public:
    // --- Public Section ---
    // Methods in this section are accessible from other parts of the code.

    // --- Lifecycle: The constructor and destructor are essential for creating and destroying APFloaterPhototools objects. ---
    APFloaterPhototools(const LLSD& key); // Constructor
    virtual ~APFloaterPhototools();        // Destructor

    // --- Overrides: The postBuild, onOpen, and onClose methods are virtual functions inherited from LLFloater and need to be public so that the viewer framework can call them. ---
    virtual bool postBuild();
    virtual void onOpen(const LLSD& key);
    virtual void onClose(bool app_quitting);

    // --- Environment Preset Management: Methods to set the sky, water, day cycle, and environment, since other parts of the code may call these methods. ---
    void setSelectedSky(const std::string& preset_name);
    void setSelectedWater(const std::string& preset_name);
    void setSelectedDayCycle(const std::string& preset_name);
    void setSelectedEnvironment();

    // --- Settings Refresh: A method for other code to request the floater be refreshed with new values. ---
    void refreshSettings();

private:
    // --- Private Section ---
    // Methods and member variables in this section are only accessible from within the APFloaterPhototools class.

    // --- UI Element Declarations: Keep all the LLComboBox, LLSlider, LLSpinCtrl, etc., member variables together. This makes it easier to see what UI elements the class manages. ---
    LLComboBox*         mWLPresetsCombo;
    LLComboBox*         mWaterPresetsCombo;
    LLComboBox*         mDayCyclePresetsCombo;
    LLSlider*           mSliderRenderShadowSplitExponentY;
    LLSpinCtrl*         mSpinnerRenderShadowSplitExponentY;
    LLSlider*           mSliderRenderShadowGaussianX;
    LLSlider*           mSliderRenderShadowGaussianY;
    LLSpinCtrl*         mSpinnerRenderShadowGaussianX;
    LLSpinCtrl*         mSpinnerRenderShadowGaussianY;
    LLSlider*           mSliderRenderSSAOEffectX;
    LLSpinCtrl*         mSpinnerRenderSSAOEffectX;
    LLSlider*           mSliderRenderSSAOEffectY;
    LLSpinCtrl*         mSpinnerRenderSSAOEffectY;
    LLSliderCtrl*       mAvatarZOffsetSlider;
    LLButton*           mBtnResetDefaults;
    LLSliderCtrl*       mMaxComplexitySlider;
    LLTextBox*          mMaxComplexityLabel;
    LLSlider*           mAnimationSpeedSlider;
    LLUICtrl*           mAnimationSpeedSpinner;
    LLCheckBoxCtrl*     mShowMySettingsCheckbox; 

    // --- Environment Management: These member variables are closely related to environment settings. ---
    LLSettingsSky::ptr_t        mLiveSky;
    LLSettingsWater::ptr_t      mLiveWater;
    LLSettingsDay::ptr_t        mLiveDay;

    // --- Connections and Slots: These member variables are related to signal and slot connections. ---
    LLEnvironment::connection_t mEnvChangedConnection;
    boost::signals2::connection mRegionChangedSlot;

    // --- Preset Loading: All the loadPresets methods are kept together. These are internal methods used to populate the UI and are not intended to be called from outside the class. ---
    void loadPresets();
    void loadDayCyclePresets(const std::multimap<std::string, LLUUID>& daycycle_map);
    void loadSkyPresets(const std::multimap<std::string, LLUUID>& sky_map);
    void loadWaterPresets(const std::multimap<std::string, LLUUID>& water_map);

    // --- Environment Preset Selection Handlers: All the onChange... methods are kept together. These methods are called when the user interacts with the environment preset combo boxes. ---
    void onChangeWaterPreset();
    void onChangeSkyPreset();
    void onChangeDayCyclePreset();
    void selectSkyPreset(const LLSD& preset);
    void selectWaterPreset(const LLSD& preset);
    void selectDayCyclePreset(const LLSD& preset);

    // --- Environment Preset Navigation Handlers: All the onClick... methods for handling the previous and next buttons are grouped together. ---
    void onClickSkyPrev();
    void onClickSkyNext();
    void onClickWaterPrev();
    void onClickWaterNext();
    void onClickDayCyclePrev();
    void onClickDayCycleNext();
    void onClickResetToRegionDefault();
    void setDefaultPresetsEnabled(bool enabled);

    // --- UI Control Event Handlers (Shadows Tab): methods that handle the events triggered by changes in the settings on the "Shadows" tab. ---
    void onChangeRenderShadowSplitExponentSliderY();
    void onChangeRenderShadowSplitExponentSpinnerY();
    void onClickResetRenderShadowSplitExponentY();
    void onChangeRenderShadowGaussianSlider();
    void onChangeRenderShadowGaussianSpinner();
    void onClickResetRenderShadowGaussianX();
    void onClickResetRenderShadowGaussianY();
    void onChangeRenderSSAOEffectSlider();
    void onChangeRenderSSAOEffectSpinner();
    void onClickResetRenderSSAOEffectX();
    void onChangeRenderSSAOEffectSliderY();
    void onChangeRenderSSAOEffectSpinnerY();
    void onClickResetRenderSSAOEffectY();                                     

    // --- UI Control Event Handlers (General Tab): methods that handle the events triggered by changes in the settings on the "General" tab. ---
    void onShowMySettingsChanged();

    // --- Complexity Management: All methods to the complexcity management and complexcity label. ---
    void updateMaxNonImpostors(const LLSD& newvalue);
    void updateMaxComplexity();
    void updateMaxComplexityLabel(const LLSD& newvalue);

    // --- Preset Helper Methods (Internal Logic):  Methods that are used internally within the class to set up and manage the floater's functionality. ---
    bool isValidPreset(const LLSD& preset);
    void stepComboBox(LLComboBox* ctrl, bool forward);
    void initCallbacks(); 

};

#endif 