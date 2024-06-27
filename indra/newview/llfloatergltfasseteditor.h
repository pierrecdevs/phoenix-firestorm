/**
 * @file llfloatergltfasseteditor.h
 * @author Andrii Kleshchev
 * @brief LLFloaterGltfAssetEditor header file
 *
 * $LicenseInfo:firstyear=2024&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2024, Linden Research, Inc.
 *
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

#ifndef LL_LLFLOATERGLTFASSETEDITOR_H
#define LL_LLFLOATERGLTFASSETEDITOR_H

#include "llfloater.h"

#include "llgltffoldermodel.h"

namespace LL
{
    namespace GLTF
    {
        class Asset;
    }
}

class LLFloaterGLTFAssetEditor : public LLFloater
{
public:
    LLFloaterGLTFAssetEditor(const LLSD& key);
    ~LLFloaterGLTFAssetEditor();

    bool postBuild() override;
    void onOpen(const LLSD& key) override;

    LLGLTFViewModel& getRootViewModel() { return mGLTFViewModel; }

    static void idle(void* user_data);
    void loadItem(S32 id, const std::string& name, LLGLTFFolderItem::EType type, LLFolderViewFolder* parent);
    void loadFromNode(S32 node, LLFolderViewFolder* parent);
    void loadFromSelection();

private:
    LLGLTFViewModel mGLTFViewModel;
    LLUIColor mUIColor;

    LLPanel* mItemListPanel = nullptr;
    LLFolderView* mFolderRoot = nullptr;
    LLScrollContainer* mScroller = nullptr;
    std::shared_ptr<LL::GLTF::Asset> mAsset;
};

#endif LL_LLFLOATERGLTFASSETEDITOR_H
