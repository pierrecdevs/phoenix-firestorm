/**
 * @file lltextbase.cpp
 * @author Martin Reddy
 * @brief The base class of text box/editor, providing Url handling support
 *
 * $LicenseInfo:firstyear=2009&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2009-2010, Linden Research, Inc.
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

#include "linden_common.h"

#include "lltextbase.h"

#include "llemojidictionary.h"
#include "llemojihelper.h"
#include "lllocalcliprect.h"
#include "llmenugl.h"
#include "llscrollcontainer.h"
#include "llspellcheck.h"
#include "llstl.h"
#include "lltextparser.h"
#include "lltextutil.h"
#include "lltooltip.h"
#include "lltrans.h"
#include "lluictrl.h"
#include "llurlaction.h"
#include "llurlregistry.h"
#include "llview.h"
#include "llwindow.h"
#include <boost/bind.hpp>
// [SL:KB] - Patch: Control-TextHighlight | Checked: 2013-12-30 (Catznip-3.6)
#include <boost/algorithm/string.hpp>
// [/SL:KB]

#include "fsregistrarutils.h"

const F32   CURSOR_FLASH_DELAY = 1.0f;  // in seconds
const S32   CURSOR_THICKNESS = 2;
const F32   TRIPLE_CLICK_INTERVAL = 0.3f;   // delay between double and triple click.

LLTextBase::line_info::line_info(S32 index_start, S32 index_end, LLRect rect, S32 line_num)
:   mDocIndexStart(index_start),
    mDocIndexEnd(index_end),
    mRect(rect),
    mLineNum(line_num)
{}

bool LLTextBase::compare_segment_end::operator()(const LLTextSegmentPtr& a, const LLTextSegmentPtr& b) const
{
    // sort empty spans (e.g. 11-11) after previous non-empty spans (e.g. 5-11)
    if (a->getEnd() == b->getEnd())
    {
        return a->getStart() < b->getStart();
    }
    else
    {
        return a->getEnd() < b->getEnd();
    }
}


// helper functors
bool LLTextBase::compare_bottom::operator()(const S32& a, const LLTextBase::line_info& b) const
{
    return a > b.mRect.mBottom; // bottom of a is higher than bottom of b
}

bool LLTextBase::compare_bottom::operator()(const LLTextBase::line_info& a, const S32& b) const
{
    return a.mRect.mBottom > b; // bottom of a is higher than bottom of b
}

bool LLTextBase::compare_bottom::operator()(const LLTextBase::line_info& a, const LLTextBase::line_info& b) const
{
    return a.mRect.mBottom > b.mRect.mBottom; // bottom of a is higher than bottom of b
}

// helper functors
bool LLTextBase::compare_top::operator()(const S32& a, const LLTextBase::line_info& b) const
{
    return a > b.mRect.mTop; // top of a is higher than top of b
}

bool LLTextBase::compare_top::operator()(const LLTextBase::line_info& a, const S32& b) const
{
    return a.mRect.mTop > b; // top of a is higher than top of b
}

bool LLTextBase::compare_top::operator()(const LLTextBase::line_info& a, const LLTextBase::line_info& b) const
{
    return a.mRect.mTop > b.mRect.mTop; // top of a is higher than top of b
}

struct LLTextBase::line_end_compare
{
    bool operator()(const S32& pos, const LLTextBase::line_info& info) const
    {
        return (pos < info.mDocIndexEnd);
    }

    bool operator()(const LLTextBase::line_info& info, const S32& pos) const
    {
        return (info.mDocIndexEnd < pos);
    }

    bool operator()(const LLTextBase::line_info& a, const LLTextBase::line_info& b) const
    {
        return (a.mDocIndexEnd < b.mDocIndexEnd);
    }

};

//////////////////////////////////////////////////////////////////////////
//
// LLTextBase
//

// register LLTextBase::Params under name "textbase"
static LLWidgetNameRegistry::StaticRegistrar sRegisterTextBaseParams(&typeid(LLTextBase::Params), "textbase");

LLTextBase::LineSpacingParams::LineSpacingParams()
:   multiple("multiple", 1.f),
    pixels("pixels", 0)
{
}

// <FS:Ansariel> Optional icon position
namespace LLInitParam
{
    void TypeValues<LLTextBaseEnums::EIconPositioning>::declareValues()
    {
        declare("left",   LLTextBaseEnums::LEFT);
        declare("right",  LLTextBaseEnums::RIGHT);
        declare("none",   LLTextBaseEnums::NONE);
    }
}
// </FS:Ansariel> Optional icon position

LLTextBase::Params::Params()
:   cursor_color("cursor_color"),
    text_color("text_color"),
    text_readonly_color("text_readonly_color"),
    text_tentative_color("text_tentative_color"),
    bg_visible("bg_visible", false),
    border_visible("border_visible", false),
    bg_readonly_color("bg_readonly_color"),
    bg_writeable_color("bg_writeable_color"),
    bg_focus_color("bg_focus_color"),
// [SL:KB] - Patch: Control-TextHighlight | Checked: 2013-12-30 (Catznip-3.6)
    bg_highlighted_color("bg_highlighted_color"),
// [/SL:KB]
    text_selected_color("text_selected_color"),
    bg_selected_color("bg_selected_color"),
    allow_scroll("allow_scroll", true),
    plain_text("plain_text",false),
    track_end("track_end", false),
    read_only("read_only", false),
    skip_link_underline("skip_link_underline", false),
    spellcheck("spellcheck", false),
    v_pad("v_pad", 0),
    h_pad("h_pad", 0),
    clip("clip", true),
    clip_partial("clip_partial", true),
    line_spacing("line_spacing"),
    max_text_length("max_length", 255),
    font_shadow("font_shadow"),
    text_valign("text_valign"),
    wrap("wrap"),
    trusted_content("trusted_content", true),
    always_show_icons("always_show_icons", false),
    use_ellipses("use_ellipses", false),
    use_emoji("use_emoji", true),
    use_color("use_color", true),
    // <FS:Ansariel> Optional icon position
    icon_positioning("icon_positioning", LLTextBaseEnums::RIGHT),
    // </FS:Ansariel> Optional icon position
    parse_urls("parse_urls", false),
    force_urls_external("force_urls_external", false),
    parse_highlights("parse_highlights", false)
{
    addSynonym(track_end, "track_bottom");
    addSynonym(wrap, "word_wrap");
    addSynonym(parse_urls, "allow_html");
}


LLTextBase::LLTextBase(const LLTextBase::Params &p)
:   LLUICtrl(p, LLTextViewModelPtr(new LLTextViewModel)),
    mURLClickSignal(NULL),
    mIsFriendSignal(NULL),
    mIsObjectBlockedSignal(NULL),
    mMaxTextByteLength( p.max_text_length ),
    mFont(p.font),
    mFontShadow(p.font_shadow),
    mPopupMenuHandle(),
    mReadOnly(p.read_only),
    mSkipTripleClick(false),
    mSkipLinkUnderline(p.skip_link_underline),
    mSpellCheck(p.spellcheck),
    mSpellCheckStart(-1),
    mSpellCheckEnd(-1),
    mCursorColor(p.cursor_color),
    mFgColor(p.text_color),
    mBorderVisible( p.border_visible ),
    mReadOnlyFgColor(p.text_readonly_color),
    mTentativeFgColor(p.text_tentative_color()),
    mWriteableBgColor(p.bg_writeable_color),
    mReadOnlyBgColor(p.bg_readonly_color),
    mFocusBgColor(p.bg_focus_color),
// [SL:KB] - Patch: Control-TextHighlight | Checked: 2013-12-30 (Catznip-3.6)
    mHighlightedBGColor(p.bg_highlighted_color),
// [/SL:KB]
    mTextSelectedColor(p.text_selected_color),
    mSelectedBGColor(p.bg_selected_color),
    mReflowIndex(S32_MAX),
    mCursorPos( 0 ),
    mScrollNeeded(false),
    mDesiredXPixel(-1),
    mHPad(p.h_pad),
    mVPad(p.v_pad),
    mHAlign(p.font_halign),
    mVAlign(p.font_valign),
    mTextVAlign(p.text_valign.isProvided() ? p.text_valign.getValue() : p.font_valign.getValue()),
    mLineSpacingMult(p.line_spacing.multiple),
    mLineSpacingPixels(p.line_spacing.pixels),
    mClip(p.clip),
    mClipPartial(p.clip_partial && !p.allow_scroll),
    mTrustedContent(p.trusted_content),
    mAlwaysShowIcons(p.always_show_icons),
    mTrackEnd( p.track_end ),
    mScrollIndex(-1),
    mSelectionStart( 0 ),
    mSelectionEnd( 0 ),
    mIsSelecting( false ),
    mPlainText ( p.plain_text ),
    mWordWrap(p.wrap),
    mUseEllipses( p.use_ellipses ),
    mUseEmoji(p.use_emoji),
    mUseColor(p.use_color),
    mParseHTML(p.parse_urls),
    mForceUrlsExternal(p.force_urls_external),
    mParseHighlights(p.parse_highlights),
    mBGVisible(p.bg_visible),
    mScroller(NULL),
    // <FS:Ansariel> Optional icon position
    mIconPositioning(p.icon_positioning),
    // </FS:Ansariel> Optional icon position
    mStyleDirty(true)
{
    if(p.allow_scroll)
    {
        LLScrollContainer::Params scroll_params;
        scroll_params.name = "text scroller";
        scroll_params.rect = getLocalRect();
        scroll_params.follows.flags = FOLLOWS_ALL;
        scroll_params.is_opaque = false;
        scroll_params.mouse_opaque = false;
        scroll_params.min_auto_scroll_rate = 200;
        scroll_params.max_auto_scroll_rate = 800;
        // <FS:Zi> Commented out to prevent contents from scrolling away while typing
        // scroll_params.border_visible = p.border_visible;
        // </FS:Zi>
        mScroller = LLUICtrlFactory::create<LLScrollContainer>(scroll_params);
        addChild(mScroller);
    }

    LLView::Params view_params;
    view_params.name = "text_contents";
    view_params.rect =  LLRect(0, 500, 500, 0);
    view_params.mouse_opaque = false;

    mDocumentView = LLUICtrlFactory::create<LLView>(view_params);
    if (mScroller)
    {
        mScroller->addChild(mDocumentView);
    }
    else
    {
        addChild(mDocumentView);
    }

    if (mSpellCheck)
    {
        LLSpellChecker::setSettingsChangeCallback(boost::bind(&LLTextBase::onSpellCheckSettingsChange, this));
    }
    mSpellCheckTimer.reset();

    createDefaultSegment();

    updateRects();
}

LLTextBase::~LLTextBase()
{
    mSegments.clear();
    LLContextMenu* menu = static_cast<LLContextMenu*>(mPopupMenuHandle.get());
    if (menu)
    {
        menu->die();
        mPopupMenuHandle.markDead();
    }
    delete mURLClickSignal;
    delete mIsFriendSignal;
    delete mIsObjectBlockedSignal;
}

void LLTextBase::initFromParams(const LLTextBase::Params& p)
{
    LLUICtrl::initFromParams(p);
    resetDirty();       // Update saved text state
    updateSegments();

    // HACK: work around enabled == readonly design bug -- RN
    // setEnabled will modify our read only status, so do this after
    // LLTextBase::initFromParams
    if (p.read_only.isProvided())
    {
        mReadOnly = p.read_only;
    }
}

bool LLTextBase::truncate()
{
    bool did_truncate = false;

    // First rough check - if we're less than 1/4th the size, we're OK
    if (getLength() >= S32(mMaxTextByteLength / 4))
    {
        // Have to check actual byte size
        S32 utf8_byte_size = 0;
        LLSD value = getViewModel()->getValue();
        if (value.type() == LLSD::TypeString)
        {
            // save a copy for strings.
            utf8_byte_size = static_cast<S32>(value.size());
        }
        else
        {
            // non string LLSDs need explicit conversion to string
            utf8_byte_size = static_cast<S32>(value.asString().size());
        }

        if ( utf8_byte_size > mMaxTextByteLength )
        {
            // Truncate safely in UTF-8
            std::string temp_utf8_text = value.asString();
            temp_utf8_text = utf8str_truncate( temp_utf8_text, mMaxTextByteLength );
            LLWString text = utf8str_to_wstring( temp_utf8_text );
            // remove extra bit of current string, to preserve formatting, etc.
            removeStringNoUndo(static_cast<S32>(text.size()), static_cast<S32>(getWText().size() - text.size()));
            did_truncate = true;
        }
    }

    return did_truncate;
}

const LLStyle::Params& LLTextBase::getStyleParams()
{
    //FIXME: convert mDefaultStyle to a flyweight http://www.boost.org/doc/libs/1_40_0/libs/flyweight/doc/index.html
    //and eliminate color member values
    if (mStyleDirty)
    {
          mStyle
                  .color(LLUIColor(&mFgColor))                      // pass linked color instead of copy of mFGColor
                  .readonly_color(LLUIColor(&mReadOnlyFgColor))
                  .selected_color(LLUIColor(&mTextSelectedColor))
                  .font(mFont)
                  .drop_shadow(mFontShadow);
          mStyleDirty = false;
    }
    return mStyle;
}

void LLTextBase::beforeValueChange()
{

}

void LLTextBase::onValueChange(S32 start, S32 end)
{
}

// [SL:KB] - Patch: Control-TextHighlight | Checked: 2013-12-30 (Catznip-3.6)
void LLTextBase::drawHighlightsBackground(const highlight_list_t& highlights, const LLColor4& color)
{
    if (!mLineInfoList.empty())
    {
        std::vector<LLRect> selection_rects;

        // Skip through the lines we aren't drawing.
        LLRect content_display_rect = getVisibleDocumentRect();

        // binary search for line that starts before top of visible buffer
        line_list_t::const_iterator line_iter = std::lower_bound(mLineInfoList.begin(), mLineInfoList.end(), content_display_rect.mTop, compare_bottom());
        line_list_t::const_iterator end_iter = std::upper_bound(mLineInfoList.begin(), mLineInfoList.end(), content_display_rect.mBottom, compare_top());
        highlight_list_t::const_iterator itHighlight = highlights.begin();

        // Find the coordinates of the selected area
        for (; (line_iter != end_iter) && (itHighlight != highlights.end()); ++line_iter)
        {
            // Find a highlight range with an end index larger than the start of this line
            while ((itHighlight != highlights.end()) && (line_iter->mDocIndexStart > itHighlight->second))
                ++itHighlight;

            // Draw all highlights on the current line
            while ((itHighlight != highlights.end()) && (itHighlight->first < line_iter->mDocIndexEnd))
            {
                // Keep the names of these to change fewer lines of LL code
                S32 selection_left = llmin(itHighlight->first, itHighlight->second);
                S32 selection_right = llmax(itHighlight->first, itHighlight->second);

                // is selection visible on this line?
                if (line_iter->mDocIndexEnd > selection_left && line_iter->mDocIndexStart < selection_right)
                {
                    segment_set_t::iterator segment_iter;
                    S32 segment_offset;
                    getSegmentAndOffset(line_iter->mDocIndexStart, &segment_iter, &segment_offset);

                    LLRect selection_rect;
                    selection_rect.mLeft = line_iter->mRect.mLeft;
                    selection_rect.mRight = line_iter->mRect.mLeft;
                    selection_rect.mBottom = line_iter->mRect.mBottom;
                    selection_rect.mTop = line_iter->mRect.mTop;

                    for (; segment_iter != mSegments.end(); ++segment_iter, segment_offset = 0)
                    {
                        LLTextSegmentPtr segmentp = *segment_iter;

                        S32 segment_line_start = segmentp->getStart() + segment_offset;
                        S32 segment_line_end = llmin(segmentp->getEnd(), line_iter->mDocIndexEnd);

                        if (segment_line_start > segment_line_end) break;

                        S32 segment_width = 0;
                        S32 segment_height = 0;

                        // if selection after beginning of segment
                        if (selection_left >= segment_line_start)
                        {
                            S32 num_chars = llmin(selection_left, segment_line_end) - segment_line_start;
                            segmentp->getDimensions(segment_offset, num_chars, segment_width, segment_height);
                            selection_rect.mLeft += segment_width;
                        }

                        // if selection_right == segment_line_end then that means we are the first character of the next segment
                        // or first character of the next line, in either case we want to add the length of the current segment
                        // to the selection rectangle and continue.
                        // if selection right > segment_line_end then selection spans end of current segment...
                        if (selection_right >= segment_line_end)
                        {
                            // extend selection slightly beyond end of line
                            // to indicate selection of newline character (use "n" character to determine width)
                            S32 num_chars = segment_line_end - segment_line_start;
                            segmentp->getDimensions(segment_offset, num_chars, segment_width, segment_height);
                            selection_rect.mRight += segment_width;
                        }
                        // else if selection ends on current segment...
                        else
                        {
                            S32 num_chars = selection_right - segment_line_start;
                            segmentp->getDimensions(segment_offset, num_chars, segment_width, segment_height);
                            selection_rect.mRight += segment_width;

                            continue;
                        }
                    }
                    selection_rects.emplace_back(selection_rect);
                }

                // Only advance if the highlight ends on the current line
                if (itHighlight->second > line_iter->mDocIndexEnd)
                    break;
                ++itHighlight;
            }
        }

        // Draw the selection box (we're using a box instead of reversing the colors on the selected text).
        gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
        F32 alpha = hasFocus() ? 0.7f : 0.3f;
        alpha *= getDrawContext().mAlpha;
        LLColor4 selection_color(color.mV[VRED], color.mV[VGREEN], color.mV[VBLUE], alpha);

        for (auto& selection_rect : selection_rects)
        {
            selection_rect.translate(mVisibleTextRect.mLeft - content_display_rect.mLeft, mVisibleTextRect.mBottom - content_display_rect.mBottom);
            gl_rect_2d(selection_rect, selection_color);
        }
    }
}
// [/SL:KB]

std::vector<LLRect> LLTextBase::getSelectionRects()
{
    // Nor supposed to be called without selection
    llassert(hasSelection());
    llassert(!mLineInfoList.empty());

    std::vector<LLRect> selection_rects;

    S32 selection_left = llmin(mSelectionStart, mSelectionEnd);
    S32 selection_right = llmax(mSelectionStart, mSelectionEnd);

    // Skip through the lines we aren't drawing.
    LLRect content_display_rect = getVisibleDocumentRect();

    // binary search for line that starts before top of visible buffer
    line_list_t::const_iterator line_iter = std::lower_bound(mLineInfoList.begin(), mLineInfoList.end(), content_display_rect.mTop, compare_bottom());
    line_list_t::const_iterator end_iter = std::upper_bound(mLineInfoList.begin(), mLineInfoList.end(), content_display_rect.mBottom, compare_top());

    bool done = false;

    // Find the coordinates of the selected area
    for (; line_iter != end_iter && !done; ++line_iter)
    {
        // is selection visible on this line?
        if (line_iter->mDocIndexEnd > selection_left && line_iter->mDocIndexStart < selection_right)
        {
            segment_set_t::iterator segment_iter;
            S32 segment_offset;
            getSegmentAndOffset(line_iter->mDocIndexStart, &segment_iter, &segment_offset);

            // Use F32 otherwise a string of multiple segments
            // will accumulate a large error
            F32 left_precise = (F32)line_iter->mRect.mLeft;
            F32 right_precise = (F32)line_iter->mRect.mLeft;

            for (; segment_iter != mSegments.end(); ++segment_iter, segment_offset = 0)
            {
                LLTextSegmentPtr segmentp = *segment_iter;

                S32 segment_line_start = segmentp->getStart() + segment_offset;
                S32 segment_line_end = llmin(segmentp->getEnd(), line_iter->mDocIndexEnd);

                if (segment_line_start > segment_line_end) break;

                F32 segment_width = 0;
                S32 segment_height = 0;

                // if selection after beginning of segment
                if (selection_left >= segment_line_start)
                {
                    S32 num_chars = llmin(selection_left, segment_line_end) - segment_line_start;
                    segmentp->getDimensionsF32(segment_offset, num_chars, segment_width, segment_height);
                    left_precise += segment_width;
                }

                // if selection_right == segment_line_end then that means we are the first character of the next segment
                // or first character of the next line, in either case we want to add the length of the current segment
                // to the selection rectangle and continue.
                // if selection right > segment_line_end then selection spans end of current segment...
                if (selection_right >= segment_line_end)
                {
                    // extend selection slightly beyond end of line
                    // to indicate selection of newline character (use "n" character to determine width)
                    S32 num_chars = segment_line_end - segment_line_start;
                    segmentp->getDimensionsF32(segment_offset, num_chars, segment_width, segment_height);
                    right_precise += segment_width;
                }
                // else if selection ends on current segment...
                else
                {
                    S32 num_chars = selection_right - segment_line_start;
                    segmentp->getDimensionsF32(segment_offset, num_chars, segment_width, segment_height);
                    right_precise += segment_width;

                    break;
                }
            }

            LLRect selection_rect;
            selection_rect.mLeft = (S32)left_precise;
            selection_rect.mRight = (S32)right_precise;
            selection_rect.mBottom = line_iter->mRect.mBottom;
            selection_rect.mTop = line_iter->mRect.mTop;

            selection_rects.push_back(selection_rect);
        }
    }

    return selection_rects;
}

std::vector<std::pair<LLRect, LLUIColor>> LLTextBase::getHighlightedBgRects()
{
    std::vector<std::pair<LLRect, LLUIColor>> highlight_rects;

    LLRect content_display_rect = getVisibleDocumentRect();

    // binary search for line that starts before top of visible buffer
    line_list_t::const_iterator line_iter =
        std::lower_bound(mLineInfoList.begin(), mLineInfoList.end(), content_display_rect.mTop, compare_bottom());
    line_list_t::const_iterator end_iter =
        std::upper_bound(mLineInfoList.begin(), mLineInfoList.end(), content_display_rect.mBottom, compare_top());

    for (; line_iter != end_iter; ++line_iter)
    {
            segment_set_t::iterator segment_iter;
            S32 segment_offset;
            getSegmentAndOffset(line_iter->mDocIndexStart, &segment_iter, &segment_offset);

            // Use F32 otherwise a string of multiple segments
            // will accumulate a large error
            F32 left_precise  = (F32)line_iter->mRect.mLeft;
            F32 right_precise = (F32)line_iter->mRect.mLeft;

            for (; segment_iter != mSegments.end(); ++segment_iter, segment_offset = 0)
            {
                LLTextSegmentPtr segmentp = *segment_iter;

                S32 segment_line_start = segmentp->getStart() + segment_offset;
                S32 segment_line_end = llmin(segmentp->getEnd(), line_iter->mDocIndexEnd);

                if (segment_line_start > segment_line_end)
                    break;

                F32 segment_width  = 0;
                S32 segment_height = 0;

                S32 num_chars = segment_line_end - segment_line_start;
                segmentp->getDimensionsF32(segment_offset, num_chars, segment_width, segment_height);
                right_precise += segment_width;

                if (segmentp->getStyle()->getDrawHighlightBg())
                {
                    LLRect selection_rect;
                    selection_rect.mLeft = (S32)left_precise;
                    selection_rect.mRight = (S32)right_precise;
                    selection_rect.mBottom = line_iter->mRect.mBottom;
                    selection_rect.mTop = line_iter->mRect.mTop;

                    highlight_rects.push_back(std::pair(selection_rect, segmentp->getStyle()->getHighlightBgColor()));
                }
                left_precise += segment_width;
            }
    }
    return highlight_rects;
}

// Draws the black box behind the selected text
void LLTextBase::drawSelectionBackground()
{
    // Draw selection even if we don't have keyboard focus for search/replace
    if (hasSelection() && !mLineInfoList.empty())
    {
        std::vector<LLRect> selection_rects = getSelectionRects();

        // Draw the selection box (we're using a box instead of reversing the colors on the selected text).
        gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
        const LLColor4& color = mSelectedBGColor;
        F32 alpha = hasFocus() ? 0.7f : 0.3f;
        alpha *= getDrawContext().mAlpha;

        LLColor4 selection_color(color.mV[VRED], color.mV[VGREEN], color.mV[VBLUE], alpha);
        LLRect content_display_rect = getVisibleDocumentRect();

        for (std::vector<LLRect>::iterator rect_it = selection_rects.begin();
            rect_it != selection_rects.end();
            ++rect_it)
        {
            LLRect selection_rect = *rect_it;
            if (mScroller)
            {
                // If scroller is On content_display_rect has correct rect and safe to use as is
                // Note: we might need to account for border
                selection_rect.translate(mVisibleTextRect.mLeft - content_display_rect.mLeft, mVisibleTextRect.mBottom - content_display_rect.mBottom);
            }
            else
            {
                // If scroller is Off content_display_rect will have rect from document, adjusted to text width, heigh and position
                // and we have to acount for offset depending on position
                S32 v_delta = 0;
                S32 h_delta = 0;
                switch (mVAlign)
                {
                case LLFontGL::TOP:
                    v_delta = mVisibleTextRect.mTop - content_display_rect.mTop - mVPad;
                    break;
                case LLFontGL::VCENTER:
                    v_delta = (llmax(mVisibleTextRect.getHeight() - content_display_rect.mTop, -content_display_rect.mBottom) + (mVisibleTextRect.mBottom - content_display_rect.mBottom)) / 2;
                    break;
                case LLFontGL::BOTTOM:
                    v_delta = mVisibleTextRect.mBottom - content_display_rect.mBottom;
                    break;
                default:
                    break;
                }
                switch (mHAlign)
                {
                case LLFontGL::LEFT:
                    h_delta = mVisibleTextRect.mLeft - content_display_rect.mLeft + mHPad;
                    break;
                case LLFontGL::HCENTER:
                    h_delta = (llmax(mVisibleTextRect.getWidth() - content_display_rect.mLeft, -content_display_rect.mRight) + (mVisibleTextRect.mRight - content_display_rect.mRight)) / 2;
                    break;
                case LLFontGL::RIGHT:
                    h_delta = mVisibleTextRect.mRight - content_display_rect.mRight;
                    break;
                default:
                    break;
                }
                selection_rect.translate(h_delta, v_delta);
            }
            gl_rect_2d(selection_rect, selection_color);
        }
    }
}

void LLTextBase::drawHighlightedBackground()
{
    if (!mLineInfoList.empty())
    {
        std::vector<std::pair<LLRect, LLUIColor>> highlight_rects = getHighlightedBgRects();

        if (highlight_rects.empty())
            return;

        gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

        LLRect content_display_rect = getVisibleDocumentRect();

        for (std::vector<std::pair<LLRect, LLUIColor>>::iterator rect_it = highlight_rects.begin();
             rect_it != highlight_rects.end(); ++rect_it)
        {
            LLRect selection_rect = rect_it->first;
            const LLColor4& color = rect_it->second;
            if (mScroller)
            {
                // If scroller is On content_display_rect has correct rect and safe to use as is
                // Note: we might need to account for border
                selection_rect.translate(mVisibleTextRect.mLeft - content_display_rect.mLeft, mVisibleTextRect.mBottom - content_display_rect.mBottom);
            }
            else
            {
                // If scroller is Off content_display_rect will have rect from document, adjusted to text width, heigh and position
                // and we have to acount for offset depending on position
                S32 v_delta = 0;
                S32 h_delta = 0;
                switch (mVAlign)
                {
                case LLFontGL::TOP:
                    v_delta = mVisibleTextRect.mTop - content_display_rect.mTop - mVPad;
                    break;
                case LLFontGL::VCENTER:
                    v_delta = (llmax(mVisibleTextRect.getHeight() - content_display_rect.mTop, -content_display_rect.mBottom) + (mVisibleTextRect.mBottom - content_display_rect.mBottom)) / 2;
                    break;
                case LLFontGL::BOTTOM:
                    v_delta = mVisibleTextRect.mBottom - content_display_rect.mBottom;
                    break;
                default:
                    break;
                }
                switch (mHAlign)
                {
                case LLFontGL::LEFT:
                    h_delta = mVisibleTextRect.mLeft - content_display_rect.mLeft + mHPad;
                    break;
                case LLFontGL::HCENTER:
                    h_delta = (llmax(mVisibleTextRect.getWidth() - content_display_rect.mLeft, -content_display_rect.mRight) + (mVisibleTextRect.mRight - content_display_rect.mRight)) / 2;
                    break;
                case LLFontGL::RIGHT:
                    h_delta = mVisibleTextRect.mRight - content_display_rect.mRight;
                    break;
                default:
                    break;
                }
                selection_rect.translate(h_delta, v_delta);
            }
            gl_rect_2d(selection_rect, color);
        }
    }
}

void LLTextBase::drawCursor()
{
    F32 alpha = getDrawContext().mAlpha;

    if( hasFocus()
        && gFocusMgr.getAppHasFocus()
        && !mReadOnly)
    {
        const LLWString &wtext = getWText();
        const llwchar* text = wtext.c_str();

        LLRect cursor_rect = getLocalRectFromDocIndex(mCursorPos);
        cursor_rect.translate(-1, 0);
        segment_set_t::iterator seg_it = getSegIterContaining(mCursorPos);

        // take style from last segment
        LLTextSegmentPtr segmentp;

        if (seg_it != mSegments.end())
        {
            segmentp = *seg_it;
        }
        else
        {
            return;
        }

        // Draw the cursor
        // (Flash the cursor every half second starting a fixed time after the last keystroke)
        F32 elapsed = mCursorBlinkTimer.getElapsedTimeF32();
        if( (elapsed < CURSOR_FLASH_DELAY ) || (S32(elapsed * 2) & 1) )
        {

            if (LL_KIM_OVERWRITE == gKeyboard->getInsertMode() && !hasSelection())
            {
                S32 segment_width = 0;
                S32 segment_height = 0;
                segmentp->getDimensions(mCursorPos - segmentp->getStart(), 1, segment_width, segment_height);
                S32 width = llmax(CURSOR_THICKNESS, segment_width);
                cursor_rect.mRight = cursor_rect.mLeft + width;
            }
            else
            {
                cursor_rect.mRight = cursor_rect.mLeft + CURSOR_THICKNESS;
            }

            gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

            LLColor4 cursor_color = mCursorColor.get() % alpha;
            gGL.color4fv( cursor_color.mV );

            gl_rect_2d(cursor_rect);

            if (LL_KIM_OVERWRITE == gKeyboard->getInsertMode() && !hasSelection() && text[mCursorPos] != '\n')
            {
                const LLFontGL* fontp;
                const LLColor4& text_color = segmentp->getColor();
                fontp = segmentp->getStyle()->getFont();
                fontp->render(text, mCursorPos, cursor_rect,
                    LLColor4(1.f - text_color.mV[VRED], 1.f - text_color.mV[VGREEN], 1.f - text_color.mV[VBLUE], alpha),
                    LLFontGL::LEFT, mTextVAlign,
                    LLFontGL::NORMAL,
                    LLFontGL::NO_SHADOW,
                    1);
            }

            // Make sure the IME is in the right place
            LLRect screen_pos = calcScreenRect();
            LLCoordGL ime_pos( screen_pos.mLeft + cursor_rect.mLeft, screen_pos.mBottom + cursor_rect.mTop );

            ime_pos.mX = (S32) (ime_pos.mX * LLUI::getScaleFactor().mV[VX]);
            ime_pos.mY = (S32) (ime_pos.mY * LLUI::getScaleFactor().mV[VY]);
            // <FS:Zi> IME - International input compositing, i.e. for Japanese / Chinese text input
#if LL_SDL2
            static LLUICachedControl<S32> sdl2_ime_default_vertical_offset("SDL2IMEDefaultVerticalOffset");
            ime_pos.mY += sdl2_ime_default_vertical_offset;
#endif
            // </FS:Zi>
            getWindow()->setLanguageTextInput( ime_pos );
        }
    }
}

void LLTextBase::drawText()
{
    S32 text_len = getLength();

    if (text_len <= 0 && mLabel.empty())
    {
        return;
    }
    else if (useLabel())
    {
        text_len = static_cast<S32>(mLabel.getWString().length());
    }

    S32 selection_left = -1;
    S32 selection_right = -1;
    // Draw selection even if we don't have keyboard focus for search/replace
    if( hasSelection())
    {
        selection_left = llmin( mSelectionStart, mSelectionEnd );
        selection_right = llmax( mSelectionStart, mSelectionEnd );
    }

    std::pair<S32, S32> line_range = getVisibleLines(mClipPartial);
    S32 first_line = line_range.first;
    S32 last_line = line_range.second;
    if (first_line >= last_line)
    {
        return;
    }

    S32 line_start = getLineStart(first_line);
    // find first text segment that spans top of visible portion of text buffer
    segment_set_t::iterator seg_iter = getSegIterContaining(line_start);
    if (seg_iter == mSegments.end())
    {
        return;
    }

    // Perform spell check if needed
    if ( (getSpellCheck()) && (getWText().length() > 2) )
    {
        // Calculate start and end indices for the spell checking range
        S32 start = line_start;
        S32 end   = getLineEnd(last_line);

        if ( (mSpellCheckStart != start) || (mSpellCheckEnd != end) )
        {
            const LLWString& wstrText = getWText();
            mMisspellRanges.clear();

            segment_set_t::const_iterator seg_it = getSegIterContaining(start);
            while (mSegments.end() != seg_it)
            {
                LLTextSegmentPtr text_segment = *seg_it;
                if ( (text_segment.isNull()) || (text_segment->getStart() >= end) )
                {
                    break;
                }

                if (!text_segment->canEdit())
                {
                    ++seg_it;
                    continue;
                }

                // Combine adjoining text segments into one
                U32 seg_start = text_segment->getStart(), seg_end = llmin(text_segment->getEnd(), end);
                while (mSegments.end() != ++seg_it)
                {
                    text_segment = *seg_it;
                    if ( (text_segment.isNull()) || (!text_segment->canEdit()) || (text_segment->getStart() >= end) )
                    {
                        break;
                    }
                    seg_end = llmin(text_segment->getEnd(), end);
                }

                // Find the start of the first word
                U32 word_start = seg_start, word_end = -1;
                U32 text_length = static_cast<U32>(wstrText.length());
                while ( (word_start < text_length) && (!LLStringOps::isAlpha(wstrText[word_start])) )
                {
                    word_start++;
                }

                // Iterate over all words in the text block and check them one by one
                while (word_start < seg_end)
                {
                    // Find the end of the current word (special case handling for "'" when it's used as a contraction)
                    word_end = word_start + 1;
                    while ( (word_end < seg_end) &&
                            ((LLWStringUtil::isPartOfWord(wstrText[word_end])) ||
                                ((L'\'' == wstrText[word_end]) &&
                                (LLStringOps::isAlnum(wstrText[word_end - 1])) && (LLStringOps::isAlnum(wstrText[word_end + 1])))) )
                    {
                        word_end++;
                    }
                    if (word_end > seg_end)
                    {
                        break;
                    }

                    if (word_start < text_length && word_end <= text_length && word_end > word_start)
                    {
                        std::string word = wstring_to_utf8str(wstrText.substr(word_start, word_end - word_start));

                        // Don't process words shorter than 3 characters
                        if ( (word.length() >= 3) && (!LLSpellChecker::instance().checkSpelling(word)) )
                        {
                            mMisspellRanges.push_back(std::pair<U32, U32>(word_start, word_end));
                        }
                    }

                    // Find the start of the next word
                    word_start = word_end + 1;
                    while ( (word_start < seg_end) && (!LLWStringUtil::isPartOfWord(wstrText[word_start])) )
                    {
                        word_start++;
                    }
                }
            }

            mSpellCheckStart = start;
            mSpellCheckEnd = end;
        }
    }
    else
    {
        mMisspellRanges.clear();
    }

    LLTextSegmentPtr cur_segment = *seg_iter;

    std::list<std::pair<U32, U32> >::const_iterator misspell_it = std::lower_bound(mMisspellRanges.begin(), mMisspellRanges.end(), std::pair<U32, U32>(line_start, 0));
    for (S32 cur_line = first_line; cur_line < last_line; cur_line++)
    {
        S32 next_line = cur_line + 1;
        line_info& line = mLineInfoList[cur_line];

        S32 next_start = -1;
        S32 line_end = text_len;

        if (next_line < getLineCount())
        {
            next_start = getLineStart(next_line);
            line_end = next_start;
        }

        LLRectf text_rect((F32)line.mRect.mLeft, (F32)line.mRect.mTop, (F32)line.mRect.mRight, (F32)line.mRect.mBottom);
        text_rect.mRight = (F32)mDocumentView->getRect().getWidth(); // clamp right edge to document extents
        text_rect.translate((F32)mDocumentView->getRect().mLeft, (F32)mDocumentView->getRect().mBottom); // adjust by scroll position

        // draw a single line of text
        S32 seg_start = line_start;
        while( seg_start < line_end )
        {
            while( cur_segment->getEnd() <= seg_start )
            {
                seg_iter++;
                if (seg_iter == mSegments.end())
                {
                    LL_WARNS() << "Ran off the segmentation end!" << LL_ENDL;

                    return;
                }
                cur_segment = *seg_iter;
            }

            S32 seg_end = llmin(line_end, cur_segment->getEnd());
            S32 clipped_end = seg_end - cur_segment->getStart();

            if (mUseEllipses                                // using ellipses
                && clipped_end == line_end                  // last segment on line
                && next_line == last_line                   // this is the last visible line
                && last_line < (S32)mLineInfoList.size())   // and there is more text to display
            {
                // more lines of text to go, but we can't fit them
                // so shrink text rect to force ellipses
                text_rect.mRight -= 2;
            }

            // Draw squiggly lines under any visible misspelled words
            while ( (mMisspellRanges.end() != misspell_it) && (misspell_it->first < (U32)seg_end) && (misspell_it->second > (U32)seg_start) )
            {
                // Skip the current word if the user is still busy editing it
                if ( (!mSpellCheckTimer.hasExpired()) && (misspell_it->first <= (U32)mCursorPos) && (misspell_it->second >= (U32)mCursorPos) )
                {
                    ++misspell_it;
                    continue;
                }

                U32 misspell_start = llmax<U32>(misspell_it->first, (U32)seg_start), misspell_end = llmin<U32>(misspell_it->second, (U32)seg_end);
                S32 squiggle_start = 0, squiggle_end = 0, pony = 0;
                cur_segment->getDimensions(seg_start - cur_segment->getStart(), misspell_start - seg_start, squiggle_start, pony);
                cur_segment->getDimensions(misspell_start - cur_segment->getStart(), misspell_end - misspell_start, squiggle_end, pony);
                squiggle_start += (S32)text_rect.mLeft;

                pony = (squiggle_end + 3) / 6;
                squiggle_start += squiggle_end / 2 - pony * 3;
                squiggle_end = squiggle_start + pony * 6;

                S32 squiggle_bottom = (S32)text_rect.mBottom + (S32)cur_segment->getStyle()->getFont()->getDescenderHeight();

                gGL.color4ub(255, 0, 0, 200);
                while (squiggle_start + 1 < squiggle_end)
                {
                    gl_line_2d(squiggle_start, squiggle_bottom, squiggle_start + 2, squiggle_bottom - 2);
                    if (squiggle_start + 3 < squiggle_end)
                    {
                        gl_line_2d(squiggle_start + 2, squiggle_bottom - 3, squiggle_start + 4, squiggle_bottom - 1);
                    }
                    squiggle_start += 4;
                }

                if (misspell_it->second > (U32)seg_end)
                {
                    break;
                }
                ++misspell_it;
            }

            text_rect.mLeft = cur_segment->draw(seg_start - cur_segment->getStart(), clipped_end, selection_left, selection_right, text_rect);

            seg_start = clipped_end + cur_segment->getStart();
        }

        line_start = next_start;
    }
}

///////////////////////////////////////////////////////////////////
// Returns change in number of characters in mWText

S32 LLTextBase::insertStringNoUndo(S32 pos, const LLWString &wstr, LLTextBase::segment_vec_t* segments )
{
    beforeValueChange();

    S32 old_len = getLength();      // length() returns character length
    S32 insert_len = static_cast<S32>(wstr.length());

    pos = getEditableIndex(pos, true);
    if (pos > old_len)
    {
        pos = old_len;
        // Should not happen,
        // if you encounter this, check where wrong position comes from
        llassert(false);
    }

    segment_set_t::iterator seg_iter = getEditableSegIterContaining(pos);

    LLTextSegmentPtr default_segment;

    LLTextSegmentPtr segmentp;
    if (seg_iter != mSegments.end())
    {
        segmentp = *seg_iter;
    }
    else
    {
        //segmentp = mSegments.back();
        return pos;
    }

    if (segmentp->canEdit())
    {
        segmentp->setEnd(segmentp->getEnd() + insert_len);
        if (seg_iter != mSegments.end())
        {
            ++seg_iter;
        }
    }
    else
    {
        // create default editable segment to hold new text
        LLStyleConstSP sp(new LLStyle(getStyleParams()));
        default_segment = new LLNormalTextSegment( sp, pos, pos + insert_len, *this);
    }

    // shift remaining segments to right
    for(;seg_iter != mSegments.end(); ++seg_iter)
    {
        LLTextSegmentPtr segmentp = *seg_iter;
        segmentp->setStart(segmentp->getStart() + insert_len);
        segmentp->setEnd(segmentp->getEnd() + insert_len);
    }

    // insert new segments
    if (segments)
    {
        if (default_segment.notNull())
        {
            // potentially overwritten by segments passed in
            insertSegment(default_segment);
        }
        for (segment_vec_t::iterator seg_iter = segments->begin();
            seg_iter != segments->end();
            ++seg_iter)
        {
            LLTextSegment* segmentp = *seg_iter;
            insertSegment(segmentp);
        }
    }

    // Insert special segments where necessary (insertSegment takes care of splitting normal text segments around them for us)
    if (mUseEmoji)
    {
        static LLUICachedControl<bool> useBWEmojis("FSUseBWEmojis", false); // <FS:Beq/> Add B&W emoji font support
        LLStyleSP emoji_style;
        LLEmojiDictionary* ed = LLEmojiDictionary::instanceExists() ? LLEmojiDictionary::getInstance() : NULL;
        for (S32 text_kitty = 0, text_len = static_cast<S32>(wstr.size()); text_kitty < text_len; text_kitty++)
        {
            llwchar code = wstr[text_kitty];
            bool isEmoji = ed ? ed->isEmoji(code) : LLStringOps::isEmoji(code);
            if (isEmoji)
            {
                if (!emoji_style)
                {
                    emoji_style = new LLStyle(getStyleParams());
                    emoji_style->setFont(LLFontGL::getFontEmojiLarge(useBWEmojis)); // <FS:Beq/> Add B&W emoji font support
                }

                S32 new_seg_start = pos + text_kitty;
                insertSegment(new LLEmojiTextSegment(emoji_style, new_seg_start, new_seg_start + 1, *this));
            }
        }
    }

    getViewModel()->getEditableDisplay().insert(pos, wstr);

    if ( truncate() )
    {
        insert_len = getLength() - old_len;
    }

    onValueChange(pos, pos + insert_len);
    needsReflow(pos);

    return insert_len;
}

S32 LLTextBase::removeStringNoUndo(S32 pos, S32 length)
{
    beforeValueChange();
    segment_set_t::iterator seg_iter = getSegIterContaining(pos);
    while(seg_iter != mSegments.end())
    {
        LLTextSegmentPtr segmentp = *seg_iter;
        S32 end = pos + length;
        if (segmentp->getStart() < pos)
        {
            // deleting from middle of segment
            if (segmentp->getEnd() > end)
            {
                segmentp->setEnd(segmentp->getEnd() - length);
            }
            // truncating segment
            else
            {
                segmentp->setEnd(pos);
            }
        }
        else if (segmentp->getStart() < end)
        {
            // deleting entire segment
            if (segmentp->getEnd() <= end)
            {
                // remove segment
                segmentp->unlinkFromDocument(this);
                segment_set_t::iterator seg_to_erase(seg_iter++);
                mSegments.erase(seg_to_erase);
                continue;
            }
            // deleting head of segment
            else
            {
                segmentp->setStart(pos);
                segmentp->setEnd(segmentp->getEnd() - length);
            }
        }
        else
        {
            // shifting segments backward to fill deleted portion
            segmentp->setStart(segmentp->getStart() - length);
            segmentp->setEnd(segmentp->getEnd() - length);
        }
        ++seg_iter;
    }

    getViewModel()->getEditableDisplay().erase(pos, length);

    // recreate default segment in case we erased everything
    createDefaultSegment();

    onValueChange(pos, pos);
    needsReflow(pos);

    return -length; // This will be wrong if someone calls removeStringNoUndo with an excessive length
}

S32 LLTextBase::overwriteCharNoUndo(S32 pos, llwchar wc)
{
    beforeValueChange();

    if (pos > (S32)getLength())
    {
        return 0;
    }
    getViewModel()->getEditableDisplay()[pos] = wc;

    onValueChange(pos, pos + 1);
    needsReflow(pos);

    return 1;
}


void LLTextBase::createDefaultSegment()
{
    // ensures that there is always at least one segment
    if (mSegments.empty())
    {
        LLStyleConstSP sp(new LLStyle(getStyleParams()));
        LLTextSegmentPtr default_segment = new LLNormalTextSegment( sp, 0, getLength() + 1, *this);
        mSegments.insert(default_segment);
        default_segment->linkToDocument(this);
    }
}

void LLTextBase::insertSegment(LLTextSegmentPtr segment_to_insert)
{
    if (segment_to_insert.isNull())
    {
        return;
    }

    segment_set_t::iterator cur_seg_iter = getSegIterContaining(segment_to_insert->getStart());
    S32 reflow_start_index = 0;

    if (cur_seg_iter == mSegments.end())
    {
        mSegments.insert(segment_to_insert);
        segment_to_insert->linkToDocument(this);
        reflow_start_index = segment_to_insert->getStart();
    }
    else
    {
        LLTextSegmentPtr cur_segmentp = *cur_seg_iter;
        reflow_start_index = cur_segmentp->getStart();
        if (cur_segmentp->getStart() < segment_to_insert->getStart())
        {
            S32 old_segment_end = cur_segmentp->getEnd();
            // split old at start point for new segment
            cur_segmentp->setEnd(segment_to_insert->getStart());
            // advance to next segment
            // insert remainder of old segment
            LLStyleConstSP sp = cur_segmentp->getStyle();
            LLTextSegmentPtr remainder_segment = new LLNormalTextSegment( sp, segment_to_insert->getStart(), old_segment_end, *this);
            mSegments.insert(cur_seg_iter, remainder_segment);
            remainder_segment->linkToDocument(this);
            // insert new segment before remainder of old segment
            mSegments.insert(cur_seg_iter, segment_to_insert);

            segment_to_insert->linkToDocument(this);
            // at this point, there will be two overlapping segments owning the text
            // associated with the incoming segment
        }
        else
        {
            mSegments.insert(cur_seg_iter, segment_to_insert);
            segment_to_insert->linkToDocument(this);
        }

        // now delete/truncate remaining segments as necessary
        // cur_seg_iter points to segment before incoming segment
        while(cur_seg_iter != mSegments.end())
        {
            cur_segmentp = *cur_seg_iter;
            if (cur_segmentp == segment_to_insert)
            {
                ++cur_seg_iter;
                continue;
            }

            if (cur_segmentp->getStart() >= segment_to_insert->getStart())
            {
                if(cur_segmentp->getEnd() <= segment_to_insert->getEnd())
                {
                    cur_segmentp->unlinkFromDocument(this);
                    // grab copy of iterator to erase, and bump it
                    segment_set_t::iterator seg_to_erase(cur_seg_iter++);
                    mSegments.erase(seg_to_erase);
                    continue;
                }
                else
                {
                    // last overlapping segment, clip to end of incoming segment
                    // and stop traversal
                    cur_segmentp->setStart(segment_to_insert->getEnd());
                    break;
                }
            }
            ++cur_seg_iter;
        }
    }

    // layout potentially changed
    needsReflow(reflow_start_index);
}

//virtual
bool LLTextBase::handleMouseDown(S32 x, S32 y, MASK mask)
{
    // handle triple click
    if (!mTripleClickTimer.hasExpired())
    {
        if (mSkipTripleClick)
        {
            return true;
        }

        S32 real_line = getLineNumFromDocIndex(mCursorPos, false);
        S32 line_start = -1;
        S32 line_end = -1;
        for (line_list_t::const_iterator it = mLineInfoList.begin(), end_it = mLineInfoList.end();
                it != end_it;
                ++it)
        {
            if (it->mLineNum < real_line)
            {
                continue;
            }
            if (it->mLineNum > real_line)
            {
                break;
            }
            if (line_start == -1)
            {
                line_start = it->mDocIndexStart;
            }
            line_end = it->mDocIndexEnd;
            line_end = llclamp(line_end, 0, getLength());
        }

        if (line_start == -1)
        {
            return true;
        }

        mSelectionEnd = line_start;
        mSelectionStart = line_end;
        setCursorPos(line_start);

        return true;
    }

    LLTextSegmentPtr cur_segment = getSegmentAtLocalPos(x, y);
    if (cur_segment && cur_segment->handleMouseDown(x, y, mask))
    {
        return true;
    }

    return LLUICtrl::handleMouseDown(x, y, mask);
}

//virtual
bool LLTextBase::handleMouseUp(S32 x, S32 y, MASK mask)
{
    LLTextSegmentPtr cur_segment = getSegmentAtLocalPos(x, y);
    if (hasMouseCapture() && cur_segment && cur_segment->handleMouseUp(x, y, mask))
    {
        // Did we just click on a link?
        if (mURLClickSignal
            && cur_segment->getStyle()
            && cur_segment->getStyle()->isLink())
        {
            // *TODO: send URL here?
            (*mURLClickSignal)(this, LLSD() );
        }
        return true;
    }

    return LLUICtrl::handleMouseUp(x, y, mask);
}

//virtual
bool LLTextBase::handleMiddleMouseDown(S32 x, S32 y, MASK mask)
{
    LLTextSegmentPtr cur_segment = getSegmentAtLocalPos(x, y);
    if (cur_segment && cur_segment->handleMiddleMouseDown(x, y, mask))
    {
        return true;
    }

    return LLUICtrl::handleMiddleMouseDown(x, y, mask);
}

//virtual
bool LLTextBase::handleMiddleMouseUp(S32 x, S32 y, MASK mask)
{
    LLTextSegmentPtr cur_segment = getSegmentAtLocalPos(x, y);
    if (cur_segment && cur_segment->handleMiddleMouseUp(x, y, mask))
    {
        return true;
    }

    return LLUICtrl::handleMiddleMouseUp(x, y, mask);
}

//virtual
bool LLTextBase::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
    LLTextSegmentPtr cur_segment = getSegmentAtLocalPos(x, y);
    if (cur_segment && cur_segment->handleRightMouseDown(x, y, mask))
    {
        return true;
    }

    return LLUICtrl::handleRightMouseDown(x, y, mask);
}

//virtual
bool LLTextBase::handleRightMouseUp(S32 x, S32 y, MASK mask)
{
    LLTextSegmentPtr cur_segment = getSegmentAtLocalPos(x, y);
    if (cur_segment && cur_segment->handleRightMouseUp(x, y, mask))
    {
        return true;
    }

    return LLUICtrl::handleRightMouseUp(x, y, mask);
}

//virtual
bool LLTextBase::handleDoubleClick(S32 x, S32 y, MASK mask)
{
    //Don't start triple click timer if user have clicked on scrollbar
    mVisibleTextRect = mScroller ? mScroller->getContentWindowRect() : getLocalRect();
    if (x >= mVisibleTextRect.mLeft && x <= mVisibleTextRect.mRight
        && y >= mVisibleTextRect.mBottom && y <= mVisibleTextRect.mTop)
    {
        mTripleClickTimer.setTimerExpirySec(TRIPLE_CLICK_INTERVAL);
    }

    LLTextSegmentPtr cur_segment = getSegmentAtLocalPos(x, y);
    if (cur_segment && cur_segment->handleDoubleClick(x, y, mask))
    {
        return true;
    }

    return LLUICtrl::handleDoubleClick(x, y, mask);
}

//virtual
bool LLTextBase::handleHover(S32 x, S32 y, MASK mask)
{
    LLTextSegmentPtr cur_segment = getSegmentAtLocalPos(x, y);
    if (cur_segment && cur_segment->handleHover(x, y, mask))
    {
        return true;
    }

    return LLUICtrl::handleHover(x, y, mask);
}

//virtual
bool LLTextBase::handleScrollWheel(S32 x, S32 y, S32 clicks)
{
    LLTextSegmentPtr cur_segment = getSegmentAtLocalPos(x, y);
    if (cur_segment && cur_segment->handleScrollWheel(x, y, clicks))
    {
        return true;
    }

    return LLUICtrl::handleScrollWheel(x, y, clicks);
}

//virtual
bool LLTextBase::handleToolTip(S32 x, S32 y, MASK mask)
{
    LLTextSegmentPtr cur_segment = getSegmentAtLocalPos(x, y);
    if (cur_segment && cur_segment->handleToolTip(x, y, mask))
    {
        return true;
    }

    return LLUICtrl::handleToolTip(x, y, mask);
}

//virtual
void LLTextBase::reshape(S32 width, S32 height, bool called_from_parent)
{
    if (width != getRect().getWidth() || height != getRect().getHeight() || LLView::sForceReshape)
    {
        bool scrolled_to_bottom = mScroller ? mScroller->isAtBottom() : false;

        LLUICtrl::reshape( width, height, called_from_parent );

        if (mScroller && scrolled_to_bottom && mTrackEnd)
        {
            // keep bottom of text buffer visible
            // do this here as well as in reflow to handle case
            // where shrinking from top, which causes buffer to temporarily
            // not be scrolled to the bottom, since the scroll index
            // specified the _top_ of the visible document region
            mScroller->goToBottom();
        }

        // do this first after reshape, because other things depend on
        // up-to-date mVisibleTextRect
        updateRects();

        needsReflow();
    }
}

//virtual
void LLTextBase::draw()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;
    // reflow if needed, on demand
    reflow();

    // then update scroll position, as cursor may have moved
    if (!mReadOnly)
    {
        updateScrollFromCursor();
    }

    LLRect text_rect;
    if (mScroller)
    {
        mScroller->localRectToOtherView(mScroller->getContentWindowRect(), &text_rect, this);
    }
    else
    {
        LLRect visible_lines_rect;
        std::pair<S32, S32> line_range = getVisibleLines(mClipPartial);
        for (S32 i = line_range.first; i < line_range.second; i++)
        {
            if (visible_lines_rect.isEmpty())
            {
                visible_lines_rect = mLineInfoList[i].mRect;
            }
            else
            {
                visible_lines_rect.unionWith(mLineInfoList[i].mRect);
            }
        }
        text_rect = visible_lines_rect;
        text_rect.translate(mDocumentView->getRect().mLeft, mDocumentView->getRect().mBottom);
    }

    if (mBGVisible)
    {
        F32 alpha = getCurrentTransparency();
        // clip background rect against extents, if we support scrolling
        LLRect bg_rect = mVisibleTextRect;
        if (mScroller)
        {
            bg_rect.intersectWith(text_rect);
        }
        const LLColor4& bg_color = mReadOnly
                            ? mReadOnlyBgColor.get()
                            : hasFocus()
                                ? mFocusBgColor.get()
                                : mWriteableBgColor.get();
        gl_rect_2d(text_rect, bg_color % alpha, true);
    }

    // Draw highlighted if needed
    if( ll::ui::SearchableControl::getHighlighted() )
    {
        const LLColor4& bg_color = ll::ui::SearchableControl::getHighlightBgColor();
        LLRect bg_rect = mVisibleTextRect;
        if( mScroller )
            bg_rect.intersectWith( text_rect );

        gl_rect_2d( text_rect, bg_color, true );
    }

    bool should_clip = mClip || mScroller != NULL;
    // <FS:Zi> Fix text bleeding at top edge of scrolling text editors
    // { LLLocalClipRect clip(text_rect, should_clip);
    {
        // unsure why the rectangle is not properly calculated, but this fixes it.
        // probably needs investigating the accuracy of:
        // - LLScrollContainer::getContentWindowRect()
        // - LLScrollContainer::localRectToOtherView()
        // - LLRect::intersectWith()
        if(text_rect.mTop>2)
        {
            text_rect.mTop-=2;
        }
        // push the modified text_rect as a GL clipping scissor on the stack
        LLLocalClipRect clip(text_rect, should_clip);
    // </FS:Zi>

        // draw document view
        if (mScroller)
        {
            drawChild(mScroller);
        }
        else
        {
            drawChild(mDocumentView);
        }

        drawHighlightedBackground();
// [SL:KB] - Patch: Control-TextHighlight | Checked: 2013-12-30 (Catznip-3.6)
        if (mHighlightsDirty)
            refreshHighlights();
        if (!mHighlights.empty())
            drawHighlightsBackground(mHighlights, mHighlightedBGColor);
// [/SL:KB]
        drawSelectionBackground();
        drawText();
        drawCursor();
    }

    mDocumentView->setVisibleDirect(false);
    LLUICtrl::draw();
    mDocumentView->setVisibleDirect(true);
}


//virtual
void LLTextBase::setColor( const LLUIColor& c )
{
    mFgColor = c;
    mStyleDirty = true;
}

//virtual
void LLTextBase::setReadOnlyColor(const LLUIColor &c)
{
    mReadOnlyFgColor = c;
    mStyleDirty = true;
}

//virtual
void LLTextBase::onVisibilityChange( bool new_visibility )
{
    LLContextMenu* menu = static_cast<LLContextMenu*>(mPopupMenuHandle.get());
    if(!new_visibility && menu)
    {
        menu->hide();
    }
    LLUICtrl::onVisibilityChange(new_visibility);
}

//virtual
void LLTextBase::setValue(const LLSD& value )
{
    static const LLStyle::Params input_params = LLStyle::Params();
    setText(value.asString(), input_params);
}

//virtual
bool LLTextBase::canDeselect() const
{
    return hasSelection();
}


//virtual
void LLTextBase::deselect()
{
    mSelectionStart = 0;
    mSelectionEnd = 0;
    mIsSelecting = false;
}

bool LLTextBase::getSpellCheck() const
{
    return (LLSpellChecker::getUseSpellCheck()) && (!mReadOnly) && (mSpellCheck);
}

const std::string& LLTextBase::getSuggestion(U32 index) const
{
    return (index < mSuggestionList.size()) ? mSuggestionList[index] : LLStringUtil::null;
}

U32 LLTextBase::getSuggestionCount() const
{
    return static_cast<U32>(mSuggestionList.size());
}

void LLTextBase::replaceWithSuggestion(U32 index)
{
    for (std::list<std::pair<U32, U32> >::const_iterator it = mMisspellRanges.begin(); it != mMisspellRanges.end(); ++it)
    {
        if ( (it->first <= (U32)mCursorPos) && (it->second >= (U32)mCursorPos) )
        {
            deselect();
            // Insert the suggestion in its place
            LLWString suggestion = utf8str_to_wstring(mSuggestionList[index]);
            LLStyleConstSP sp(new LLStyle(getStyleParams()));
            LLTextSegmentPtr segmentp = new LLNormalTextSegment(sp, it->first, it->first + static_cast<S32>(suggestion.size()), *this);
            segment_vec_t segments(1, segmentp);
            insertStringNoUndo(it->first, suggestion, &segments);

            // Delete the misspelled word
            removeStringNoUndo(it->first + (S32)suggestion.length(), it->second - it->first);


            setCursorPos(it->first + (S32)suggestion.length());
            onSpellCheckPerformed();

            break;
        }
    }
    mSpellCheckStart = mSpellCheckEnd = -1;
}

void LLTextBase::addToDictionary()
{
    if (canAddToDictionary())
    {
        LLSpellChecker::instance().addToCustomDictionary(getMisspelledWord(mCursorPos));
    }
}

bool LLTextBase::canAddToDictionary() const
{
    return (getSpellCheck()) && (isMisspelledWord(mCursorPos));
}

void LLTextBase::addToIgnore()
{
    if (canAddToIgnore())
    {
        LLSpellChecker::instance().addToIgnoreList(getMisspelledWord(mCursorPos));
    }
}

bool LLTextBase::canAddToIgnore() const
{
    return (getSpellCheck()) && (isMisspelledWord(mCursorPos));
}

std::string LLTextBase::getMisspelledWord(U32 pos) const
{
    for (std::list<std::pair<U32, U32> >::const_iterator it = mMisspellRanges.begin(); it != mMisspellRanges.end(); ++it)
    {
        if ( (it->first <= pos) && (it->second >= pos) )
        {
            return wstring_to_utf8str(getWText().substr(it->first, it->second - it->first));
        }
    }
    return LLStringUtil::null;
}

bool LLTextBase::isMisspelledWord(U32 pos) const
{
    for (std::list<std::pair<U32, U32> >::const_iterator it = mMisspellRanges.begin(); it != mMisspellRanges.end(); ++it)
    {
        if ( (it->first <= pos) && (it->second >= pos) )
        {
            return true;
        }
    }
    return false;
}

void LLTextBase::onSpellCheckSettingsChange()
{
    // Recheck the spelling on every change
    mMisspellRanges.clear();
    mSpellCheckStart = mSpellCheckEnd = -1;
}

void LLTextBase::onFocusReceived()
{
    LLUICtrl::onFocusReceived();
    if (!getLength() && !mLabel.empty())
    {
        // delete label which is LLLabelTextSegment
        clearSegments();
    }
}

void LLTextBase::onFocusLost()
{
    LLUICtrl::onFocusLost();
    if (!getLength() && !mLabel.empty())
    {
        resetLabel();
    }
}

// Sets the scrollbar from the cursor position
void LLTextBase::updateScrollFromCursor()
{
    // Update scroll position even in read-only mode (when there's no cursor displayed)
    // because startOfDoc()/endOfDoc() modify cursor position. See EXT-736.

    if (!mScrollNeeded || !mScroller)
    {
        return;
    }
    mScrollNeeded = false;

    // scroll so that the cursor is at the top of the page
    LLRect scroller_doc_window = getVisibleDocumentRect();
    LLRect cursor_rect_doc = getDocRectFromDocIndex(mCursorPos);
    mScroller->scrollToShowRect(cursor_rect_doc, LLRect(0, scroller_doc_window.getHeight() - 5, scroller_doc_window.getWidth(), 5));
}

S32 LLTextBase::getLeftOffset(S32 width)
{
    switch (mHAlign)
    {
    case LLFontGL::LEFT:
        return mHPad;
    case LLFontGL::HCENTER:
        return mHPad + llmax(0, (mVisibleTextRect.getWidth() - width - mHPad) / 2);
    case LLFontGL::RIGHT:
        {
            // Font's rendering rounds string size, if value gets rounded
            // down last symbol might not have enough space to render,
            // compensate by adding an extra pixel as padding
            const S32 right_padding = 1;
            return llmax(mHPad, mVisibleTextRect.getWidth() - width - right_padding);
        }
    default:
        return mHPad;
    }
}

void LLTextBase::reflow()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;

    updateSegments();

    if (mReflowIndex == S32_MAX)
    {
        return;
    }

    bool scrolled_to_bottom = mScroller ? mScroller->isAtBottom() : false;

    LLRect cursor_rect = getLocalRectFromDocIndex(mCursorPos);
    bool follow_selection = getLocalRect().overlaps(cursor_rect); // cursor is (potentially) visible

    // store in top-left relative coordinates to avoid issues with horizontal scrollbar appearing and disappearing
    cursor_rect.mTop = mVisibleTextRect.mTop - cursor_rect.mTop;
    cursor_rect.mBottom = mVisibleTextRect.mTop - cursor_rect.mBottom;

    S32 first_line = getFirstVisibleLine();

    // if scroll anchor not on first line, update it to first character of first line
    if ((first_line < mLineInfoList.size())
        &&  (mScrollIndex <  mLineInfoList[first_line].mDocIndexStart
            ||  mScrollIndex >= mLineInfoList[first_line].mDocIndexEnd))
    {
        mScrollIndex = mLineInfoList[first_line].mDocIndexStart;
    }
    LLRect first_char_rect = getLocalRectFromDocIndex(mScrollIndex);
    // store in top-left relative coordinates to avoid issues with horizontal scrollbar appearing and disappearing
    first_char_rect.mTop = mVisibleTextRect.mTop - first_char_rect.mTop;
    first_char_rect.mBottom = mVisibleTextRect.mTop - first_char_rect.mBottom;

    S32 reflow_count = 0;
    while(mReflowIndex < S32_MAX)
    {
        // we can get into an infinite loop if the document height does not monotonically increase
        // with decreasing width (embedded ui elements with alternate layouts).  In that case,
        // we want to stop reflowing after 2 iterations.  We use 2, since we need to handle the case
        // of introducing a vertical scrollbar causing a reflow with less width.  We should also always
        // use an even number of iterations to avoid user visible oscillation of the layout
        if(++reflow_count > 2)
        {
            LL_DEBUGS() << "Breaking out of reflow due to possible infinite loop in " << getName() << LL_ENDL;
            break;
        }

        S32 start_index = mReflowIndex;
        mReflowIndex = S32_MAX;

        // shrink document to minimum size (visible portion of text widget)
        // to force inlined widgets with follows set to shrink
        if (mWordWrap)
        {
            mDocumentView->reshape(mVisibleTextRect.getWidth(), mDocumentView->getRect().getHeight());
        }

        S32 cur_top = 0;

        segment_set_t::iterator seg_iter = mSegments.begin();
        S32 seg_offset = 0;
        S32 line_start_index = 0;
        const F32 text_available_width = (F32)(mVisibleTextRect.getWidth() - mHPad);  // reserve room for margin
        F32 remaining_pixels = text_available_width;
        S32 line_count = 0;

        // find and erase line info structs starting at start_index and going to end of document
        if (!mLineInfoList.empty())
        {
            // find first element whose end comes after start_index
            line_list_t::iterator iter = std::upper_bound(mLineInfoList.begin(), mLineInfoList.end(), start_index, line_end_compare());
            if (iter != mLineInfoList.end())
            {
                line_start_index = iter->mDocIndexStart;
                line_count = iter->mLineNum;
                cur_top = iter->mRect.mTop;
                getSegmentAndOffset(iter->mDocIndexStart, &seg_iter, &seg_offset);
                mLineInfoList.erase(iter, mLineInfoList.end());
            }
        }

        S32 line_height = 0;
        S32 seg_line_offset = line_count + 1;

        while(seg_iter != mSegments.end())
        {
            LLTextSegmentPtr segment = *seg_iter;

            // track maximum height of any segment on this line
            S32 cur_index = segment->getStart() + seg_offset;

            // ask segment how many character fit in remaining space
            S32 character_count = segment->getNumChars(getWordWrap() ? llmax(0, ll_round(remaining_pixels)) : S32_MAX,
                                                        seg_offset,
                                                        cur_index - line_start_index,
                                                        S32_MAX,
                                                        line_count - seg_line_offset);

            F32 segment_width;
            S32 segment_height;
            bool force_newline = segment->getDimensionsF32(seg_offset, character_count, segment_width, segment_height);
            // grow line height as necessary based on reported height of this segment
            line_height = llmax(line_height, segment_height);
            remaining_pixels -= segment_width;

            seg_offset += character_count;

            S32 last_segment_char_on_line = segment->getStart() + seg_offset;

            // Note: make sure text will fit in width - use ceil, but also make sure
            // ceil is used only once per line
            S32 text_actual_width = llceil(text_available_width - remaining_pixels);
            S32 text_left = getLeftOffset(text_actual_width);
            LLRect line_rect(text_left,
                            cur_top,
                            text_left + text_actual_width,
                            cur_top - line_height);

            // if we didn't finish the current segment...
            if (last_segment_char_on_line < segment->getEnd())
            {
                // add line info and keep going
                mLineInfoList.push_back(line_info(
                                            line_start_index,
                                            last_segment_char_on_line,
                                            line_rect,
                                            line_count));

                line_start_index = segment->getStart() + seg_offset;
                cur_top -= ll_round((F32)line_height * mLineSpacingMult) + mLineSpacingPixels;
                remaining_pixels = text_available_width;
                line_height = 0;
            }
            // ...just consumed last segment..
            else if (++segment_set_t::iterator(seg_iter) == mSegments.end())
            {
                mLineInfoList.push_back(line_info(
                                            line_start_index,
                                            last_segment_char_on_line,
                                            line_rect,
                                            line_count));
                cur_top -= ll_round((F32)line_height * mLineSpacingMult) + mLineSpacingPixels;
                break;
            }
            // ...or finished a segment and there are segments remaining on this line
            else
            {
                // subtract pixels used and increment segment
                if (force_newline)
                {
                    mLineInfoList.push_back(line_info(
                                                line_start_index,
                                                last_segment_char_on_line,
                                                line_rect,
                                                line_count));
                    line_start_index = segment->getStart() + seg_offset;
                    cur_top -= ll_round((F32)line_height * mLineSpacingMult) + mLineSpacingPixels;
                    line_height = 0;
                    remaining_pixels = text_available_width;
                }
                ++seg_iter;
                seg_offset = 0;
                seg_line_offset = force_newline ? line_count + 1 : line_count;
            }
            if (force_newline)
            {
                line_count++;
            }
        }

        // calculate visible region for diplaying text
        updateRects();

        for (segment_set_t::iterator segment_it = mSegments.begin();
            segment_it != mSegments.end();
            ++segment_it)
        {
            LLTextSegmentPtr segmentp = *segment_it;
            segmentp->updateLayout(*this);

        }
    }

    // apply scroll constraints after reflowing text
    if (!hasMouseCapture() && mScroller)
    {
        if (scrolled_to_bottom && mTrackEnd)
        {
            // keep bottom of text buffer visible
            endOfDoc();
        }
        else if (hasSelection() && follow_selection)
        {
            // keep cursor in same vertical position on screen when selecting text
            LLRect new_cursor_rect_doc = getDocRectFromDocIndex(mCursorPos);
            LLRect old_cursor_rect = cursor_rect;
            old_cursor_rect.mTop = mVisibleTextRect.mTop - cursor_rect.mTop;
            old_cursor_rect.mBottom = mVisibleTextRect.mTop - cursor_rect.mBottom;

            mScroller->scrollToShowRect(new_cursor_rect_doc, old_cursor_rect);
        }
        else
        {
            // keep first line of text visible
            LLRect new_first_char_rect = getDocRectFromDocIndex(mScrollIndex);

            // pass in desired rect in the coordinate frame of the document viewport
            LLRect old_first_char_rect = first_char_rect;
            old_first_char_rect.mTop = mVisibleTextRect.mTop - first_char_rect.mTop;
            old_first_char_rect.mBottom = mVisibleTextRect.mTop - first_char_rect.mBottom;

            mScroller->scrollToShowRect(new_first_char_rect, old_first_char_rect);
        }
    }

    // reset desired x cursor position
    updateCursorXPos();
}

LLRect LLTextBase::getTextBoundingRect()
{
    reflow();
    return mTextBoundingRect;
}


void LLTextBase::clearSegments()
{
    mSegments.clear();
    createDefaultSegment();
}

S32 LLTextBase::getLineStart( S32 line ) const
{
    S32 num_lines = getLineCount();
    if (num_lines == 0)
    {
        return 0;
    }

    line = llclamp(line, 0, num_lines-1);
    return mLineInfoList[line].mDocIndexStart;
}

S32 LLTextBase::getLineEnd( S32 line ) const
{
    S32 num_lines = getLineCount();
    if (num_lines == 0)
    {
        return 0;
    }

    line = llclamp(line, 0, num_lines-1);
    return mLineInfoList[line].mDocIndexEnd;
}



S32 LLTextBase::getLineNumFromDocIndex( S32 doc_index, bool include_wordwrap) const
{
    if (mLineInfoList.empty())
    {
        return 0;
    }
    else
    {
        line_list_t::const_iterator iter = std::upper_bound(mLineInfoList.begin(), mLineInfoList.end(), doc_index, line_end_compare());
        if (include_wordwrap)
        {
            return (S32)(iter - mLineInfoList.begin());
        }
        else
        {
            if (iter == mLineInfoList.end())
            {
                return mLineInfoList.back().mLineNum;
            }
            else
            {
                return iter->mLineNum;
            }
        }
    }
}

// Given an offset into text (pos), find the corresponding line (from the start of the doc) and an offset into the line.
S32 LLTextBase::getLineOffsetFromDocIndex( S32 startpos, bool include_wordwrap) const
{
    if (mLineInfoList.empty())
    {
        return startpos;
    }
    else
    {
        line_list_t::const_iterator iter = std::upper_bound(mLineInfoList.begin(), mLineInfoList.end(), startpos, line_end_compare());
        return startpos - iter->mDocIndexStart;
    }
}

S32 LLTextBase::getFirstVisibleLine() const
{
    LLRect visible_region = getVisibleDocumentRect();

    // binary search for line that starts before top of visible buffer
    line_list_t::const_iterator iter = std::lower_bound(mLineInfoList.begin(), mLineInfoList.end(), visible_region.mTop, compare_bottom());

    return (S32)(iter - mLineInfoList.begin());
}

std::pair<S32, S32> LLTextBase::getVisibleLines(bool require_fully_visible)
{
    LLRect visible_region = getVisibleDocumentRect();
    line_list_t::const_iterator first_iter;
    line_list_t::const_iterator last_iter;

    // make sure we have an up-to-date mLineInfoList
    reflow();

    if (require_fully_visible)
    {
        first_iter = std::lower_bound(mLineInfoList.begin(), mLineInfoList.end(), visible_region.mTop, compare_top());
        last_iter = std::upper_bound(mLineInfoList.begin(), mLineInfoList.end(), visible_region.mBottom, compare_bottom());
    }
    else
    {
        first_iter = std::upper_bound(mLineInfoList.begin(), mLineInfoList.end(), visible_region.mTop, compare_bottom());
        last_iter = std::lower_bound(mLineInfoList.begin(), mLineInfoList.end(), visible_region.mBottom, compare_top());
    }
    return std::pair<S32, S32>((S32)(first_iter - mLineInfoList.begin()), (S32)(last_iter - mLineInfoList.begin()));
}



LLTextViewModel* LLTextBase::getViewModel() const
{
    return (LLTextViewModel*)mViewModel.get();
}

void LLTextBase::addDocumentChild(LLView* view)
{
    mDocumentView->addChild(view);
}

void LLTextBase::removeDocumentChild(LLView* view)
{
    mDocumentView->removeChild(view);
}


void LLTextBase::updateSegments()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;
    createDefaultSegment();
}

void LLTextBase::getSegmentAndOffset( S32 startpos, segment_set_t::const_iterator* seg_iter, S32* offsetp ) const
{
    *seg_iter = getSegIterContaining(startpos);
    if (*seg_iter == mSegments.end())
    {
        *offsetp = 0;
    }
    else
    {
        *offsetp = startpos - (**seg_iter)->getStart();
    }
}

void LLTextBase::getSegmentAndOffset( S32 startpos, segment_set_t::iterator* seg_iter, S32* offsetp )
{
    *seg_iter = getSegIterContaining(startpos);
    if (*seg_iter == mSegments.end())
    {
        *offsetp = 0;
    }
    else
    {
        *offsetp = startpos - (**seg_iter)->getStart();
    }
}

LLTextBase::segment_set_t::iterator LLTextBase::getEditableSegIterContaining(S32 index)
{
    segment_set_t::iterator it = getSegIterContaining(index);
    segment_set_t::iterator orig_it = it;

    if (it == mSegments.end()) return it;

    if (!(*it)->canEdit()
        && index == (*it)->getStart()
        && it != mSegments.begin())
    {
        it--;
        if ((*it)->canEdit())
        {
            return it;
        }
    }
    return orig_it;
}

LLTextBase::segment_set_t::const_iterator LLTextBase::getEditableSegIterContaining(S32 index) const
{
    segment_set_t::const_iterator it = getSegIterContaining(index);
    segment_set_t::const_iterator orig_it = it;
    if (it == mSegments.end()) return it;

    if (!(*it)->canEdit()
        && index == (*it)->getStart()
        && it != mSegments.begin())
    {
        it--;
        if ((*it)->canEdit())
        {
            return it;
        }
    }
    return orig_it;
}

LLTextBase::segment_set_t::iterator LLTextBase::getSegIterContaining(S32 index)
{
    static LLPointer<LLIndexSegment> index_segment = new LLIndexSegment();

    // when there are no segments, we return the end iterator, which must be checked by caller
    if (mSegments.size() <= 1) { return mSegments.begin(); }

    index_segment->setStart(index);
    index_segment->setEnd(index);
    segment_set_t::iterator it = mSegments.upper_bound(index_segment);
    return it;
}

LLTextBase::segment_set_t::const_iterator LLTextBase::getSegIterContaining(S32 index) const
{
    static LLPointer<LLIndexSegment> index_segment = new LLIndexSegment();

    // when there are no segments, we return the end iterator, which must be checked by caller
    if (mSegments.size() <= 1) { return mSegments.begin(); }

    index_segment->setStart(index);
    index_segment->setEnd(index);
    LLTextBase::segment_set_t::const_iterator it =  mSegments.upper_bound(index_segment);

    return it;
}

// Finds the text segment (if any) at the give local screen position
LLTextSegmentPtr LLTextBase::getSegmentAtLocalPos( S32 x, S32 y, bool hit_past_end_of_line)
{
    if (!hasMouseCapture() && !mVisibleTextRect.pointInRect(x, y))
    {
        return LLTextSegmentPtr();
    }

    // Find the cursor position at the requested local screen position
    S32 offset = getDocIndexFromLocalCoord( x, y, false, hit_past_end_of_line);
    segment_set_t::iterator seg_iter = getSegIterContaining(offset);
    if (seg_iter != mSegments.end())
    {
        return *seg_iter;
    }
    else
    {
        return LLTextSegmentPtr();
    }
}

void LLTextBase::createUrlContextMenu(S32 x, S32 y, const std::string &in_url)
{
    // work out the XUI menu file to use for this url
    LLUrlMatch match;
    std::string url = in_url;

    // <FS:Ansariel> FIRE-30805: Replace "agentself" with "agent" when copying SLUrls
    LLStringUtil::replaceString(url, "/agentself/", "/agent/");

    if (! LLUrlRegistry::instance().findUrl(url, match))
    {
        return;
    }

    std::string xui_file = match.getMenuName();
    if (xui_file.empty())
    {
        return;
    }

    // set up the callbacks for all of the potential menu items, N.B. we
    // don't use const ref strings in callbacks in case url goes out of scope
    LLUICtrl::CommitCallbackRegistry::ScopedRegistrar registrar;
    registrar.add("Url.Open", boost::bind(&LLUrlAction::openURL, url));
    registrar.add("Url.OpenInternal", boost::bind(&LLUrlAction::openURLInternal, url));
    registrar.add("Url.OpenExternal", boost::bind(&LLUrlAction::openURLExternal, url));
    registrar.add("Url.Execute", boost::bind(&LLUrlAction::executeSLURL, url, true));
    registrar.add("Url.Block", boost::bind(&LLUrlAction::blockObject, url));
    registrar.add("Url.Unblock", boost::bind(&LLUrlAction::unblockObject, url));
    registrar.add("Url.Teleport", boost::bind(&LLUrlAction::teleportToLocation, url));
    registrar.add("Url.ShowProfile", boost::bind(&LLUrlAction::showProfile, url));
    registrar.add("Url.AddFriend", boost::bind(&LLUrlAction::addFriend, url));
    registrar.add("Url.RemoveFriend", boost::bind(&LLUrlAction::removeFriend, url));
    registrar.add("Url.ReportAbuse", boost::bind(&LLUrlAction::reportAbuse, url));
    registrar.add("Url.SendIM", boost::bind(&LLUrlAction::sendIM, url));
    registrar.add("Url.ShowOnMap", boost::bind(&LLUrlAction::showLocationOnMap, url));
    registrar.add("Url.CopyLabel", boost::bind(&LLUrlAction::copyLabelToClipboard, url));
    registrar.add("Url.CopyUrl", boost::bind(&LLUrlAction::copyURLToClipboard, url));

    // <FS:Ansariel> Additional convenience options
    std::string target_id_str = LLUrlAction::extractUuidFromSlurl(url).asString();
    registrar.add("FS.ZoomIn", std::bind(&LLUrlAction::executeSLURL, "secondlife:///app/firestorm/" + target_id_str + "/zoom", true));
    registrar.add("FS.TeleportToTarget", std::bind(&LLUrlAction::executeSLURL, "secondlife:///app/firestorm/" + target_id_str + "/teleportto", true));
    registrar.add("FS.OfferTeleport", std::bind(&LLUrlAction::executeSLURL, "secondlife:///app/firestorm/" + target_id_str + "/offerteleport", true));
    registrar.add("FS.RequestTeleport", std::bind(&LLUrlAction::executeSLURL, "secondlife:///app/firestorm/" + target_id_str + "/requestteleport", true));
    registrar.add("FS.TrackAvatar", std::bind(&LLUrlAction::executeSLURL, "secondlife:///app/firestorm/" + target_id_str + "/track", true));
    registrar.add("FS.AddToContactSet", std::bind(&LLUrlAction::executeSLURL, "secondlife:///app/firestorm/" + target_id_str + "/addtocontactset", true));  // [FS:CR]
    registrar.add("FS.BlockAvatar", std::bind(&LLUrlAction::executeSLURL, "secondlife:///app/firestorm/" + target_id_str + "/blockavatar", true));
    registrar.add("FS.ViewLog", std::bind(&LLUrlAction::executeSLURL, "secondlife:///app/firestorm/" + target_id_str + "/viewlog", true));
    // </FS:Ansariel>

    // <FS:Ansariel> Add enable checks for menu items
    LLUICtrl::EnableCallbackRegistry::ScopedRegistrar enable_registrar;
    LLUUID target_id(target_id_str);
    enable_registrar.add("Url.EnableShowProfile", std::bind(&FSRegistrarUtils::checkIsEnabled, gFSRegistrarUtils, target_id, EFSRegistrarFunctionActionType::FS_RGSTR_ACT_SHOW_PROFILE));
    enable_registrar.add("Url.EnableAddFriend", std::bind(&FSRegistrarUtils::checkIsEnabled, gFSRegistrarUtils, target_id, EFSRegistrarFunctionActionType::FS_RGSTR_ACT_ADD_FRIEND));
    enable_registrar.add("Url.EnableRemoveFriend", std::bind(&FSRegistrarUtils::checkIsEnabled, gFSRegistrarUtils, target_id, EFSRegistrarFunctionActionType::FS_RGSTR_ACT_REMOVE_FRIEND));
    enable_registrar.add("Url.EnableSendIM", std::bind(&FSRegistrarUtils::checkIsEnabled, gFSRegistrarUtils, target_id, EFSRegistrarFunctionActionType::FS_RGSTR_ACT_SEND_IM));
    enable_registrar.add("FS.EnableZoomIn", std::bind(&FSRegistrarUtils::checkIsEnabled, gFSRegistrarUtils, target_id, EFSRegistrarFunctionActionType::FS_RGSTR_ACT_ZOOM_IN));
    enable_registrar.add("FS.EnableOfferTeleport", std::bind(&FSRegistrarUtils::checkIsEnabled, gFSRegistrarUtils, target_id, EFSRegistrarFunctionActionType::FS_RGSTR_ACT_OFFER_TELEPORT));
    enable_registrar.add("FS.EnableTrackAvatar", std::bind(&FSRegistrarUtils::checkIsEnabled, gFSRegistrarUtils, target_id, EFSRegistrarFunctionActionType::FS_RGSTR_ACT_TRACK_AVATAR));
    enable_registrar.add("FS.EnableTeleportToTarget", std::bind(&FSRegistrarUtils::checkIsEnabled, gFSRegistrarUtils, target_id, EFSRegistrarFunctionActionType::FS_RGSTR_ACT_TELEPORT_TO));
    enable_registrar.add("FS.EnableRequestTeleport", std::bind(&FSRegistrarUtils::checkIsEnabled, gFSRegistrarUtils, target_id, EFSRegistrarFunctionActionType::FS_RGSTR_ACT_REQUEST_TELEPORT));
    enable_registrar.add("FS.CheckIsAgentBlocked", std::bind(&FSRegistrarUtils::checkIsEnabled, gFSRegistrarUtils, target_id, EFSRegistrarFunctionActionType::FS_RGSTR_CHK_AVATAR_BLOCKED));
    enable_registrar.add("FS.EnableBlockAvatar", std::bind(&FSRegistrarUtils::checkIsEnabled, gFSRegistrarUtils, target_id, EFSRegistrarFunctionActionType::FS_RGSTR_CHK_IS_NOT_SELF));
    enable_registrar.add("FS.EnableViewLog", std::bind(&FSRegistrarUtils::checkIsEnabled, gFSRegistrarUtils, target_id, EFSRegistrarFunctionActionType::FS_RGSTR_ACT_VIEW_TRANSCRIPT));
    // </FS:Ansariel>

    // <FS:Zi> FIRE-30725 - Add more group functions to group URL context menu
    registrar.add("FS.JoinGroup", std::bind(&LLUrlAction::executeSLURL, "secondlife:///app/firestorm/" + target_id_str + "/groupjoin", true));
    registrar.add("FS.LeaveGroup", std::bind(&LLUrlAction::executeSLURL, "secondlife:///app/firestorm/" + target_id_str + "/groupleave", true));
    registrar.add("FS.ActivateGroup", std::bind(&LLUrlAction::executeSLURL, "secondlife:///app/firestorm/" + target_id_str + "/groupactivate", true));

    // hook up moderation tools
    // if someone knows how to pass a parameter to these, let me know! - Zi
    // also, I wish I could pass the group session id through these calls, but LLTextBase does not know it - Zi
    registrar.add("FS.AllowGroupChat", std::bind(&LLUrlAction::executeSLURL, "secondlife:///app/firestorm/" + target_id_str + "/groupchatallow", true));
    registrar.add("FS.ForbidGroupChat", std::bind(&LLUrlAction::executeSLURL, "secondlife:///app/firestorm/" + target_id_str + "/groupchatforbid", true));
    registrar.add("FS.EjectGroupMember", std::bind(&LLUrlAction::executeSLURL, "secondlife:///app/firestorm/" + target_id_str + "/groupeject", true));
    registrar.add("FS.BanGroupMember", std::bind(&LLUrlAction::executeSLURL, "secondlife:///app/firestorm/" + target_id_str + "/groupban", true));

    enable_registrar.add("FS.WaitingForGroupData", std::bind(&FSRegistrarUtils::checkIsEnabled, gFSRegistrarUtils, target_id, EFSRegistrarFunctionActionType::FS_RGSTR_CHK_WAITING_FOR_GROUP_DATA));
    enable_registrar.add("FS.HaveGroupData", std::bind(&FSRegistrarUtils::checkIsEnabled, gFSRegistrarUtils, target_id, EFSRegistrarFunctionActionType::FS_RGSTR_CHK_HAVE_GROUP_DATA));
    enable_registrar.add("FS.EnableJoinGroup", std::bind(&FSRegistrarUtils::checkIsEnabled, gFSRegistrarUtils, target_id, EFSRegistrarFunctionActionType::FS_RGSTR_CHK_CAN_JOIN_GROUP));
    enable_registrar.add("FS.EnableLeaveGroup", std::bind(&FSRegistrarUtils::checkIsEnabled, gFSRegistrarUtils, target_id, EFSRegistrarFunctionActionType::FS_RGSTR_CHK_CAN_LEAVE_GROUP));
    enable_registrar.add("FS.EnableActivateGroup", std::bind(&FSRegistrarUtils::checkIsEnabled, gFSRegistrarUtils, target_id, EFSRegistrarFunctionActionType::FS_RGSTR_CHK_GROUP_NOT_ACTIVE));
    // </FS:Zi>

    // create and return the context menu from the XUI file

    LLContextMenu* menu = static_cast<LLContextMenu*>(mPopupMenuHandle.get());
    if (menu)
    {
        menu->die();
        mPopupMenuHandle.markDead();
    }
    llassert(LLMenuGL::sMenuContainer != NULL);
    menu = LLUICtrlFactory::getInstance()->createFromFile<LLContextMenu>(xui_file, LLMenuGL::sMenuContainer,
                                                                         LLMenuHolderGL::child_registry_t::instance());
    if (menu)
    {
        mPopupMenuHandle = menu->getHandle();

        if (mIsFriendSignal)
        {
            bool isFriend = *(*mIsFriendSignal)(LLUUID(LLUrlAction::getUserID(url)));
            LLView* addFriendButton = menu->findChild<LLView>("add_friend");
            LLView* removeFriendButton = menu->findChild<LLView>("remove_friend");

            if (addFriendButton && removeFriendButton)
            {
                addFriendButton->setEnabled(!isFriend);
                removeFriendButton->setEnabled(isFriend);
            }
        }

        if (mIsObjectBlockedSignal)
        {
            bool is_blocked = *(*mIsObjectBlockedSignal)(LLUUID(LLUrlAction::getObjectId(url)), LLUrlAction::getObjectName(url));
            LLView* blockButton = menu->findChild<LLView>("block_object");
            LLView* unblockButton = menu->findChild<LLView>("unblock_object");

            if (blockButton && unblockButton)
            {
                blockButton->setVisible(!is_blocked);
                unblockButton->setVisible(is_blocked);
            }
        }

        // <FS:Zi> hide the moderation tools in the context menu unless we are in a group IM floater
        LLFloater* parent_floater = getParentByType<LLFloater>();
        if (!parent_floater || parent_floater->getName() != "panel_im")
        {
            menu->getChild<LLView>("GroupModerationSubmenu")->setVisible(false);
            menu->getChild<LLView>("GroupModerationSeparator")->setVisible(false);
        }
        // </FS:Zi>

        menu->show(x, y);
        LLMenuGL::showPopup(this, menu, x, y);
    }
}

void LLTextBase::setText(const LLStringExplicit &utf8str, const LLStyle::Params& input_params)
{
    // clear out the existing text and segments
    getViewModel()->setDisplay(LLWStringUtil::null);

    clearSegments();
//  createDefaultSegment();

    deselect();

    // append the new text (supports Url linking)
    std::string text(utf8str);
    LLStringUtil::removeCRLF(text);

    // appendText modifies mCursorPos...
    appendText(text, false, input_params);
    // ...so move cursor to top after appending text
    if (!mTrackEnd)
    {
        startOfDoc();
    }

    onValueChange(0, getLength());
}

// virtual
const std::string& LLTextBase::getText() const
{
    return getViewModel()->getStringValue();
}

// IDEVO - icons can be UI image names or UUID sent from
// server with avatar display name
static LLUIImagePtr image_from_icon_name(const std::string& icon_name)
{
    if (LLUUID::validate(icon_name))
    {
        return LLUI::getUIImageByID( LLUUID(icon_name) );
    }
    else
    {
        return LLUI::getUIImage(icon_name);
    }
}


void LLTextBase::appendTextImpl(const std::string& new_text, const LLStyle::Params& input_params, bool force_slurl)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;
    LLStyle::Params style_params(getStyleParams());
    style_params.overwriteFrom(input_params);

    S32 part = (S32)LLTextParser::WHOLE;
    if ((mParseHTML || force_slurl) && !style_params.is_link) // Don't search for URLs inside a link segment (STORM-358).
    {
        S32 start=0,end=0;
        LLUrlMatch match;
        std::string text = new_text;
        while (LLUrlRegistry::instance().findUrl(text, match,
                boost::bind(&LLTextBase::replaceUrl, this, _1, _2, _3), isContentTrusted() || mAlwaysShowIcons, force_slurl))
        {
            start = match.getStart();
            end = match.getEnd()+1;

            LLStyle::Params link_params(style_params);
            // <FS:Ansariel> Overwrite only if we explicitly allow it
            //link_params.overwriteFrom(match.getStyle());
            if (input_params.use_default_link_style)
            {
                link_params.overwriteFrom(match.getStyle());
            }
            // </FS:Ansariel>

            // output the text before the Url
            if (start > 0)
            {
                if (part == (S32)LLTextParser::WHOLE ||
                    part == (S32)LLTextParser::START)
                {
                    part = (S32)LLTextParser::START;
                }
                else
                {
                    part = (S32)LLTextParser::MIDDLE;
                }
                std::string subtext=text.substr(0,start);
                appendAndHighlightText(subtext, part, style_params);
            }

            // add icon before url if need
            // <FS:Ansariel> Optional icon position
            //LLTextUtil::processUrlMatch(&match, this, isContentTrusted() || match.isTrusted() || mAlwaysShowIcons);
            //if ((isContentTrusted() || match.isTrusted()) && !match.getIcon().empty() )
            //{
            //  setLastSegmentToolTip(LLTrans::getString("TooltipSLIcon"));
            //}
            if (mIconPositioning == LLTextBaseEnums::LEFT || match.isTrusted() || mAlwaysShowIcons)
            {
                LLTextUtil::processUrlMatch(&match, this, isContentTrusted() || match.isTrusted() || mAlwaysShowIcons);
                if ((isContentTrusted() || match.isTrusted()) && !match.getIcon().empty() )
                {
                    setLastSegmentToolTip(LLTrans::getString("TooltipSLIcon"));
                }
            }
            // </FS:Ansariel> Optional icon position

            // output the styled Url
            // <FS:CR> FIRE-11437 - Don't supress font style for chat history name links
            //appendAndHighlightTextImpl(match.getLabel(), part, link_params, match.getUnderline());
            appendAndHighlightTextImpl(match.getLabel(), part, link_params,
                                       input_params.can_underline_on_hover ? match.getUnderline() : LLStyle::UNDERLINE_NEVER);
            // </FS:CR>
            bool tooltip_required =  !match.getTooltip().empty();

            // set the tooltip for the Url label
            if (tooltip_required)
            {
                setLastSegmentToolTip(match.getTooltip());
            }

            // show query part of url with gray color only for LLUrlEntryHTTP url entries
            std::string label = match.getQuery();
            if (label.size())
            {
                // <FS:Ansariel> Custom URI query part color
                //link_params.color = LLColor4::grey;
                //link_params.readonly_color = LLColor4::grey;
                //appendAndHighlightTextImpl(label, part, link_params, match.getUnderline());
                static LLUIColor query_part_color = LLUIColorTable::getInstance()->getColor("UriQueryPartColor", LLColor4::grey);
                link_params.color = query_part_color;
                link_params.readonly_color = query_part_color;
                appendAndHighlightTextImpl(label, part, link_params, input_params.can_underline_on_hover ? match.getUnderline() : LLStyle::UNDERLINE_NEVER);
                // </FS:Ansariel>

                // set the tooltip for the query part of url
                if (tooltip_required)
                {
                    setLastSegmentToolTip(match.getTooltip());
                }
            }

            // <FS:Ansariel> Optional icon position
            if (mIconPositioning == LLTextBaseEnums::RIGHT && !match.isTrusted() && !mAlwaysShowIcons)
            {
                LLTextUtil::processUrlMatch(&match,this,isContentTrusted());
            }
            // </FS:Ansariel> Optional icon position

            // move on to the rest of the text after the Url
            if (end < (S32)text.length())
            {
                text = text.substr(end,text.length() - end);
                end=0;
                part=(S32)LLTextParser::END;
            }
            else
            {
                break;
            }
        }
        if (part != (S32)LLTextParser::WHOLE)
            part=(S32)LLTextParser::END;
        if (end < (S32)text.length())
            appendAndHighlightText(text, part, style_params);
    }
    else
    {
        appendAndHighlightText(new_text, part, style_params);
    }
}

void LLTextBase::setLastSegmentToolTip(const std::string &tooltip)
{
    segment_set_t::iterator it = getSegIterContaining(getLength()-1);
    if (it != mSegments.end())
    {
        LLTextSegmentPtr segment = *it;
        segment->setToolTip(tooltip);
    }
}

void LLTextBase::appendText(const std::string &new_text, bool prepend_newline, const LLStyle::Params& input_params)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;
    if (new_text.empty())
        return;

    if(prepend_newline)
        appendLineBreakSegment(input_params);
    appendTextImpl(new_text,input_params);
}

void LLTextBase::setLabel(const LLStringExplicit& label)
{
    mLabel = label;
    resetLabel();
}

bool LLTextBase::setLabelArg(const std::string& key, const LLStringExplicit& text )
{
    mLabel.setArg(key, text);
    return true;
}

void LLTextBase::resetLabel()
{
    if (useLabel())
    {
        clearSegments();

        LLStyle* style = new LLStyle(getStyleParams());
        style->setColor(mTentativeFgColor);
        LLStyleConstSP sp(style);

        LLTextSegmentPtr label = new LLLabelTextSegment(sp, 0, static_cast<S32>(mLabel.getWString().length()) + 1, *this);
        insertSegment(label);
    }
}

bool LLTextBase::useLabel() const
{
    return !getLength() && !mLabel.empty() && !hasFocus();
}

void LLTextBase::setFont(const LLFontGL* font)
{
    mFont = font;
    mStyleDirty = true;

    // <FS:Ansariel> FIRE-29425: User-selectable font and size for notecards
    for (auto segment : mSegments)
    {
        LLStyleConstSP style = segment->getStyle();
        LLStyleSP new_style(new LLStyle(*style));
        new_style->setFont(mFont);
        LLStyleConstSP sp(new_style);
        segment->setStyle(sp);
    }
    needsReflow();
    // </FS:Ansariel>
}

void LLTextBase::needsReflow(S32 index)
{
    LL_DEBUGS() << "reflow on object " << (void*)this << " index = " << mReflowIndex << ", new index = " << index << LL_ENDL;
    mReflowIndex = llmin(mReflowIndex, index);

// [SL:KB] - Patch: Control-TextHighlight | Checked: 2013-12-30 (Catznip-3.6)
    mHighlightsDirty = true;
// [/SL:KB]
}

S32 LLTextBase::removeFirstLine()
{
    if (!mLineInfoList.empty())
    {
        S32 length = getLineEnd(0);
        deselect();
        removeStringNoUndo(0, length);
        return length;
    }
    return 0;
}

// virtual
void LLTextBase::copyContents(const LLTextBase* source)
{
    llassert(source);
    if (!source)
        return;

    beforeValueChange();
    deselect();

    mSegments.clear();
    for (const LLTextSegmentPtr& segp : source->mSegments)
    {
        mSegments.emplace(segp->clone(*this));
    }

    mLineInfoList.clear();
    for (const line_info& li : mLineInfoList)
    {
        mLineInfoList.push_back(line_info(li));
    }

    getViewModel()->setDisplay(source->getViewModel()->getDisplay());

    onValueChange(0, getLength());
    needsReflow();
}

void LLTextBase::appendLineBreakSegment(const LLStyle::Params& style_params)
{
    segment_vec_t segments;
    LLStyleConstSP sp(new LLStyle(style_params));
    segments.push_back(new LLLineBreakTextSegment(sp, getLength()));

    insertStringNoUndo(getLength(), utf8str_to_wstring("\n"), &segments);
}

void LLTextBase::appendImageSegment(const LLStyle::Params& style_params)
{
    if(getPlainText())
    {
        return;
    }
    segment_vec_t segments;
    LLStyleConstSP sp(new LLStyle(style_params));
    segments.push_back(new LLImageTextSegment(sp, getLength(),*this));

    insertStringNoUndo(getLength(), utf8str_to_wstring(" "), &segments);
}

void LLTextBase::appendWidget(const LLInlineViewSegment::Params& params, const std::string& text, bool allow_undo)
{
    segment_vec_t segments;
    LLWString widget_wide_text = utf8str_to_wstring(text);
    segments.push_back(new LLInlineViewSegment(params, getLength(), getLength() + static_cast<S32>(widget_wide_text.size())));

    insertStringNoUndo(getLength(), widget_wide_text, &segments);
}

void LLTextBase::appendAndHighlightTextImpl(const std::string &new_text, S32 highlight_part, const LLStyle::Params& style_params, e_underline underline_link)
{
    // Save old state
    S32 selection_start = mSelectionStart;
    S32 selection_end = mSelectionEnd;
    bool was_selecting = mIsSelecting;
    S32 cursor_pos = mCursorPos;
    S32 old_length = getLength();
    bool cursor_was_at_end = (mCursorPos == old_length);

    deselect();

    setCursorPos(old_length);

    if (mParseHighlights)
    {
        LLStyle::Params highlight_params(style_params);

        auto pieces = LLTextParser::instance().parsePartialLineHighlights(new_text, highlight_params.color, (LLTextParser::EHighlightPosition)highlight_part);
        for (S32 i = 0; i < pieces.size(); i++)
        {
            const auto& piece_pair = pieces[i];
            highlight_params.color = piece_pair.second;

            LLWString wide_text;
            wide_text = utf8str_to_wstring(piece_pair.first);

            S32 cur_length = getLength();
            LLStyleConstSP sp(new LLStyle(highlight_params));
            LLTextSegmentPtr segmentp;
            if ((underline_link == e_underline::UNDERLINE_ON_HOVER) || mSkipLinkUnderline)
            {
                // <FS:Ansariel> Only reset underline font style
                //highlight_params.font.style("NORMAL");
                std::string normal_font_style(highlight_params.font.style());
                LLStringUtil::replaceString(normal_font_style, "UNDERLINE", "");
                if (normal_font_style.empty())
                {
                    normal_font_style = "NORMAL";
                }
                highlight_params.font.style(normal_font_style);
                // </FS:Ansariel>
                LLStyleConstSP normal_sp(new LLStyle(highlight_params));
                segmentp = new LLOnHoverChangeableTextSegment(sp, normal_sp, cur_length, cur_length + static_cast<S32>(wide_text.size()), *this);
            }
            else
            {
                segmentp = new LLNormalTextSegment(sp, cur_length, cur_length + static_cast<S32>(wide_text.size()), *this);
            }
            segment_vec_t segments;
            segments.push_back(segmentp);
            insertStringNoUndo(cur_length, wide_text, &segments);
        }
    }
    else
    {
        LLWString wide_text;
        wide_text = utf8str_to_wstring(new_text);

        segment_vec_t segments;
        S32 segment_start = old_length;
        S32 segment_end = old_length + static_cast<S32>(wide_text.size());
        LLStyleConstSP sp(new LLStyle(style_params));
        if ((underline_link == e_underline::UNDERLINE_ON_HOVER) || mSkipLinkUnderline)
        {
            LLStyle::Params normal_style_params(style_params);
            normal_style_params.font.style("NORMAL");
            LLStyleConstSP normal_sp(new LLStyle(normal_style_params));
            segments.push_back(new LLOnHoverChangeableTextSegment(sp, normal_sp, segment_start, segment_end, *this));
        }
        else
        {
            segments.push_back(new LLNormalTextSegment(sp, segment_start, segment_end, *this));
        }

        insertStringNoUndo(getLength(), wide_text, &segments);
    }

    // Set the cursor and scroll position
    if (selection_start != selection_end)
    {
        mSelectionStart = selection_start;
        mSelectionEnd = selection_end;

        mIsSelecting = was_selecting;
        setCursorPos(cursor_pos);
    }
    else if (cursor_was_at_end)
    {
        setCursorPos(getLength());
    }
    else
    {
        setCursorPos(cursor_pos);
    }
}

void LLTextBase::appendAndHighlightText(const std::string &new_text, S32 highlight_part, const LLStyle::Params& style_params, e_underline underline_link)
{
    if (new_text.empty())
    {
        return;
    }

    std::string::size_type start = 0;
    std::string::size_type pos = new_text.find("\n", start);

    while (pos != std::string::npos)
    {
        if (pos != start)
        {
            std::string str = std::string(new_text,start,pos-start);
            appendAndHighlightTextImpl(str, highlight_part, style_params, underline_link);
        }
        appendLineBreakSegment(style_params);
        start = pos+1;
        pos = new_text.find("\n", start);
    }

    std::string str = std::string(new_text, start, new_text.length() - start);
    appendAndHighlightTextImpl(str, highlight_part, style_params, underline_link);
}


void LLTextBase::replaceUrl(const std::string &url,
                            const std::string &label,
                            const std::string &icon)
{
    // get the full (wide) text for the editor so we can change it
    LLWString text = getWText();
    LLWString wlabel = utf8str_to_wstring(label);
    bool modified = false;
    S32 seg_start = 0;

    // iterate through each segment looking for ones styled as links
    segment_set_t::iterator it;
    for (it = mSegments.begin(); it != mSegments.end(); ++it)
    {
        LLTextSegment *seg = *it;
        LLStyleConstSP style = seg->getStyle();

        // update segment start/end length in case we replaced text earlier
        S32 seg_length = seg->getEnd() - seg->getStart();
        seg->setStart(seg_start);
        seg->setEnd(seg_start + seg_length);

        // if we find a link with our Url, then replace the label
        if (style->getLinkHREF() == url)
        {
            S32 start = seg->getStart();
            S32 end = seg->getEnd();
            text = text.substr(0, start) + wlabel + text.substr(end, text.size() - end + 1);
            seg->setEnd(start + static_cast<S32>(wlabel.size()));
            modified = true;
        }

        // Icon might be updated when more avatar or group info
        // becomes available
        if (style->isImage() && style->getLinkHREF() == url)
        {
            LLUIImagePtr image = image_from_icon_name( icon );
            if (image)
            {
                LLStyle::Params icon_params;
                icon_params.image = image;
                LLStyleConstSP new_style(new LLStyle(icon_params));
                seg->setStyle(new_style);
                modified = true;
            }
        }

        // work out the character offset for the next segment
        seg_start = seg->getEnd();
    }

    // update the editor with the new (wide) text string
    if (modified)
    {
        getViewModel()->setDisplay(text);
        // <FS:Ansariel> FIRE-20214: Text gets deselected upon avatar/group name received callback
        //deselect();
        // Make sure we're still within limits
        S32 text_length = getLength();
        mSelectionStart = llmin(mSelectionStart, static_cast<S32>(text_length));
        mSelectionEnd = llmin(mSelectionEnd, static_cast<S32>(text_length));
        // </FS:Ansariel>
        setCursorPos(mCursorPos);
        needsReflow();
    }
}


void LLTextBase::setWText(const LLWString& text)
{
    setText(wstring_to_utf8str(text));
}

const LLWString& LLTextBase::getWText() const
{
    return getViewModel()->getDisplay();
}

S32 LLTextBase::getTextGeneration() const
{
    return getViewModel()->getDisplayGeneration();
}

// If round is true, if the position is on the right half of a character, the cursor
// will be put to its right.  If round is false, the cursor will always be put to the
// character's left.

S32 LLTextBase::getDocIndexFromLocalCoord( S32 local_x, S32 local_y, bool round, bool hit_past_end_of_line) const
{
    // Figure out which line we're nearest to.
    LLRect doc_rect = mDocumentView->getRect();
    S32 doc_y = local_y - doc_rect.mBottom;

    // binary search for line that starts before local_y
    line_list_t::const_iterator line_iter = std::lower_bound(mLineInfoList.begin(), mLineInfoList.end(), doc_y, compare_bottom());

    if (!mLineInfoList.size() || line_iter == mLineInfoList.end())
    {
        return getLength(); // past the end
    }

    S32 pos = getLength();
    F32 start_x = (F32)(line_iter->mRect.mLeft + doc_rect.mLeft);

    segment_set_t::iterator line_seg_iter;
    S32 line_seg_offset;
    for(getSegmentAndOffset(line_iter->mDocIndexStart, &line_seg_iter, &line_seg_offset);
        line_seg_iter != mSegments.end();
        ++line_seg_iter, line_seg_offset = 0)
    {
        const LLTextSegmentPtr segmentp = *line_seg_iter;

        S32 segment_line_start = segmentp->getStart() + line_seg_offset;
        S32 segment_line_length = llmin(segmentp->getEnd(), line_iter->mDocIndexEnd) - segment_line_start;
        F32 text_width;
        S32 text_height;
        bool newline = segmentp->getDimensionsF32(line_seg_offset, segment_line_length, text_width, text_height);

        if(newline)
        {
            pos = segment_line_start + segmentp->getOffset(local_x - (S32)start_x, line_seg_offset, segment_line_length, round);
            break;
        }

        // if we've reached a line of text *below* the mouse cursor, doc index is first character on that line
        if (hit_past_end_of_line && doc_y > line_iter->mRect.mTop)
        {
            pos = segment_line_start;
            break;
        }
        if (local_x < start_x + text_width)         // cursor to left of right edge of text
        {
            // Figure out which character we're nearest to.
            S32 offset;
            if (!segmentp->canEdit())
            {
                F32 segment_width;
                S32 segment_height;
                segmentp->getDimensionsF32(0, segmentp->getEnd() - segmentp->getStart(), segment_width, segment_height);
                if (round && local_x - start_x > segment_width / 2)
                {
                    offset = segment_line_length;
                }
                else
                {
                    offset = 0;
                }
            }
            else
            {
                offset = segmentp->getOffset(local_x - (S32)start_x, line_seg_offset, segment_line_length, round);
            }
            pos = segment_line_start + offset;
            break;
        }
        else if (hit_past_end_of_line && segmentp->getEnd() >= line_iter->mDocIndexEnd)
        {
            if (getLineNumFromDocIndex(line_iter->mDocIndexEnd - 1) == line_iter->mLineNum)
            {
                // if segment wraps to the next line we should step one char back
                // to compensate for the space char between words
                // which is removed due to wrapping
                pos = llclamp(line_iter->mDocIndexEnd - 1, 0, getLength());
            }
            else
            {
                pos = llclamp(line_iter->mDocIndexEnd, 0, getLength());
            }
            break;
        }
        start_x += text_width;
    }

    return pos;
}

// returns rectangle of insertion caret
// in document coordinate frame from given index into text
LLRect LLTextBase::getDocRectFromDocIndex(S32 pos) const
{
    if (mLineInfoList.empty())
    {
        return LLRect();
    }

    // clamp pos to valid values
    pos = llclamp(pos, 0, mLineInfoList.back().mDocIndexEnd - 1);

    line_list_t::const_iterator line_iter = std::upper_bound(mLineInfoList.begin(), mLineInfoList.end(), pos, line_end_compare());

    segment_set_t::iterator line_seg_iter;
    S32 line_seg_offset;
    segment_set_t::iterator cursor_seg_iter;
    S32 cursor_seg_offset;
    getSegmentAndOffset(line_iter->mDocIndexStart, &line_seg_iter, &line_seg_offset);
    getSegmentAndOffset(pos, &cursor_seg_iter, &cursor_seg_offset);

    F32 doc_left_precise = (F32)line_iter->mRect.mLeft;

    while(line_seg_iter != mSegments.end())
    {
        const LLTextSegmentPtr segmentp = *line_seg_iter;

        if (line_seg_iter == cursor_seg_iter)
        {
            // cursor advanced to right based on difference in offset of cursor to start of line
            F32 segment_width;
            S32 segment_height;
            segmentp->getDimensionsF32(line_seg_offset, cursor_seg_offset - line_seg_offset, segment_width, segment_height);
            doc_left_precise += segment_width;

            break;
        }
        else
        {
            // add remainder of current text segment to cursor position
            F32 segment_width;
            S32 segment_height;
            segmentp->getDimensionsF32(line_seg_offset, (segmentp->getEnd() - segmentp->getStart()) - line_seg_offset, segment_width, segment_height);
            doc_left_precise += segment_width;
            // offset will be 0 for all segments after the first
            line_seg_offset = 0;
            // go to next text segment on this line
            ++line_seg_iter;
        }
    }

    LLRect doc_rect;
    doc_rect.mLeft = (S32)doc_left_precise;
    doc_rect.mBottom = line_iter->mRect.mBottom;
    doc_rect.mTop = line_iter->mRect.mTop;

    // set rect to 0 width
    doc_rect.mRight = doc_rect.mLeft;

    return doc_rect;
}

LLRect LLTextBase::getLocalRectFromDocIndex(S32 pos) const
{
    LLRect content_window_rect = mScroller ? mScroller->getContentWindowRect() : getLocalRect();
    if (mBorderVisible)
    {
        // <FS:Zi> Commented out to prevent contents from scrolling away while typing
        // content_window_rect.stretch(-1);
        // </FS:Zi>
    }

    LLRect local_rect;

    if (mLineInfoList.empty())
    {
        // return default height rect in upper left
        local_rect = content_window_rect;
        local_rect.mBottom = local_rect.mTop - mFont->getLineHeight();
        return local_rect;
    }

    // get the rect in document coordinates
    LLRect doc_rect = getDocRectFromDocIndex(pos);

    // compensate for scrolled, inset view of doc
    LLRect scrolled_view_rect = getVisibleDocumentRect();
    local_rect = doc_rect;
    local_rect.translate(content_window_rect.mLeft - scrolled_view_rect.mLeft,
                        content_window_rect.mBottom - scrolled_view_rect.mBottom);

    return local_rect;
}

void LLTextBase::updateCursorXPos()
{
    // reset desired x cursor position
    mDesiredXPixel = getLocalRectFromDocIndex(mCursorPos).mLeft;
}

//virtual
void LLTextBase::startOfLine()
{
    S32 offset = getLineOffsetFromDocIndex(mCursorPos);
    setCursorPos(mCursorPos - offset);
}

void LLTextBase::endOfLine()
{
    S32 line = getLineNumFromDocIndex(mCursorPos);
    S32 num_lines = getLineCount();
    if (line + 1 >= num_lines)
    {
        setCursorPos(getLength());
    }
    else
    {
        setCursorPos( getLineStart(line + 1) - 1 );
    }
}

void LLTextBase::startOfDoc()
{
    setCursorPos(0);
    if (mScroller)
    {
        mScroller->goToTop();
    }
}

void LLTextBase::endOfDoc()
{
    setCursorPos(getLength());
    if (mScroller)
    {
        mScroller->goToBottom();
    }
}

void LLTextBase::changePage( S32 delta )
{
    const S32 PIXEL_OVERLAP_ON_PAGE_CHANGE = 10;
    if (delta == 0 || !mScroller) return;

    LLRect cursor_rect = getLocalRectFromDocIndex(mCursorPos);

    if( delta == -1 )
    {
        mScroller->pageUp(PIXEL_OVERLAP_ON_PAGE_CHANGE);
    }
    else
    if( delta == 1 )
    {
        mScroller->pageDown(PIXEL_OVERLAP_ON_PAGE_CHANGE);
    }

    if (getLocalRectFromDocIndex(mCursorPos) == cursor_rect)
    {
        // cursor didn't change apparent position, so move to top or bottom of document, respectively
        if (delta < 0)
        {
            startOfDoc();
        }
        else
        {
            endOfDoc();
        }
    }
    else
    {
        setCursorAtLocalPos(cursor_rect.getCenterX(), cursor_rect.getCenterY(), true, false);
    }
}

// Picks a new cursor position based on the screen size of text being drawn.
void LLTextBase::setCursorAtLocalPos( S32 local_x, S32 local_y, bool round, bool keep_cursor_offset )
{
    setCursorPos(getDocIndexFromLocalCoord(local_x, local_y, round), keep_cursor_offset);
}


void LLTextBase::changeLine( S32 delta )
{
    S32 line = getLineNumFromDocIndex(mCursorPos);
    S32 max_line_nb = getLineCount() - 1;
    max_line_nb = (max_line_nb < 0 ? 0 : max_line_nb);

    S32 new_line = llclamp(line + delta, 0, max_line_nb);

    if (new_line != line)
    {
        LLRect visible_region = getVisibleDocumentRect();
        S32 new_cursor_pos = getDocIndexFromLocalCoord(mDesiredXPixel,
                                                       mLineInfoList[new_line].mRect.mBottom + mVisibleTextRect.mBottom - visible_region.mBottom, true);
        S32 actual_line = getLineNumFromDocIndex(new_cursor_pos);
        if (actual_line != new_line)
        {
            // line edge, correcting position by 1 to move onto proper line
            new_cursor_pos += new_line - actual_line;
        }
        setCursorPos(new_cursor_pos, true);
    }
}

bool LLTextBase::scrolledToStart()
{
    return mScroller->isAtTop();
}

bool LLTextBase::scrolledToEnd()
{
    return mScroller->isAtBottom();
}

// [SL:KB] - Patch: Control-TextHighlight | Checked: 2013-12-30 (Catznip-3.6)
void LLTextBase::clearHighlights()
{
    mHighlightWord.clear();
    mHighlights.clear();
    mHighlightsDirty = false;
}

void LLTextBase::refreshHighlights()
{
    if (mHighlightsDirty)
    {
        mHighlights.clear();
        if (!mHighlightWord.empty())
        {
            const LLWString& wstrText = getWText();

            std::list<boost::iterator_range<LLWString::const_iterator> > highlightRanges;
            if (mHighlightCaseInsensitive)
                boost::ifind_all(highlightRanges, wstrText, mHighlightWord);
            else
                boost::find_all(highlightRanges, wstrText, mHighlightWord);

            for (std::list<boost::iterator_range<LLWString::const_iterator> >::const_iterator itRange = highlightRanges.begin(); itRange != highlightRanges.end(); ++itRange)
            {
                S32 idxStart = (S32)(itRange->begin() - wstrText.begin());
                mHighlights.emplace_back(range_pair_t(idxStart, idxStart + static_cast<S32>(itRange->size())));
            }
        }
        mHighlightsDirty = false;
    }
}

void LLTextBase::setHighlightWord(const std::string& strHighlight, bool fCaseInsensitive)
{
    if (strHighlight.empty())
    {
        clearHighlights();
        return;
    }

    mHighlightWord = utf8str_to_wstring(strHighlight);
    mHighlightCaseInsensitive = fCaseInsensitive;
    mHighlightsDirty = true;
}
// [/SL:KB]

bool LLTextBase::setCursor(S32 row, S32 column)
{
    if (row < 0 || column < 0) return false;

    S32 n_lines = static_cast<S32>(mLineInfoList.size());
    for (S32 line = row; line < n_lines; ++line)
    {
        const line_info& li = mLineInfoList[line];

        if (li.mLineNum < row)
        {
            continue;
        }
        else if (li.mLineNum > row)
        {
            break; // invalid column specified
        }

        // Found the given row.
        S32 line_length = li.mDocIndexEnd - li.mDocIndexStart;;
        if (column >= line_length)
        {
            column -= line_length;
            continue;
        }

        // Found the given column.
        updateCursorXPos();
        S32 doc_pos = li.mDocIndexStart + column;
        return setCursorPos(doc_pos);
    }

    return false; // invalid row or column specified
}


bool LLTextBase::setCursorPos(S32 cursor_pos, bool keep_cursor_offset)
{
    S32 new_cursor_pos = cursor_pos;
    if (new_cursor_pos != mCursorPos)
    {
        new_cursor_pos = getEditableIndex(new_cursor_pos, new_cursor_pos >= mCursorPos);
    }

    mCursorPos = llclamp(new_cursor_pos, 0, (S32)getLength());
    needsScroll();
    if (!keep_cursor_offset)
        updateCursorXPos();
    // did we get requested position?
    return new_cursor_pos == cursor_pos;
}

// constraint cursor to editable segments of document
S32 LLTextBase::getEditableIndex(S32 index, bool increasing_direction)
{
    segment_set_t::iterator segment_iter;
    S32 offset;
    getSegmentAndOffset(index, &segment_iter, &offset);
    if (segment_iter == mSegments.end())
    {
        return 0;
    }

    LLTextSegmentPtr segmentp = *segment_iter;

    if (segmentp->canEdit())
    {
        return segmentp->getStart() + offset;
    }
    else if (segmentp->getStart() < index && index < segmentp->getEnd())
    {
        // bias towards document end
        if (increasing_direction)
        {
            return segmentp->getEnd();
        }
        // bias towards document start
        else
        {
            return segmentp->getStart();
        }
    }
    else
    {
        return index;
    }
}

void LLTextBase::updateRects()
{
    LLRect old_text_rect = mVisibleTextRect;
    mVisibleTextRect = mScroller ? mScroller->getContentWindowRect() : getLocalRect();

    if (mLineInfoList.empty())
    {
        mTextBoundingRect = LLRect(0, mVPad, mHPad, 0);
    }
    else
    {
        mTextBoundingRect = mLineInfoList.begin()->mRect;
        for (line_list_t::const_iterator line_iter = ++mLineInfoList.begin();
            line_iter != mLineInfoList.end();
            ++line_iter)
        {
            mTextBoundingRect.unionWith(line_iter->mRect);
        }

        mTextBoundingRect.mTop += mVPad;

        S32 delta_pos = 0;

        switch(mVAlign)
        {
        case LLFontGL::TOP:
            delta_pos = llmax(mVisibleTextRect.getHeight() - mTextBoundingRect.mTop, -mTextBoundingRect.mBottom);
            break;
        case LLFontGL::VCENTER:
            delta_pos = (llmax(mVisibleTextRect.getHeight() - mTextBoundingRect.mTop, -mTextBoundingRect.mBottom) + (mVisibleTextRect.mBottom - mTextBoundingRect.mBottom)) / 2;
            break;
        case LLFontGL::BOTTOM:
            delta_pos = mVisibleTextRect.mBottom - mTextBoundingRect.mBottom;
            break;
        case LLFontGL::BASELINE:
            // do nothing
            break;
        }
        // move line segments to fit new document rect
        for (line_list_t::iterator it = mLineInfoList.begin(); it != mLineInfoList.end(); ++it)
        {
            it->mRect.translate(0, delta_pos);
        }
        mTextBoundingRect.translate(0, delta_pos);
    }

    // update document container dimensions according to text contents
    LLRect doc_rect;
    // use old mVisibleTextRect constraint document to width of viewable region
    doc_rect.mBottom = llmin(mVisibleTextRect.mBottom,  mTextBoundingRect.mBottom);
    doc_rect.mLeft = 0;

    // allow horizontal scrolling?
    // if so, use entire width of text contents
    // otherwise, stop at width of mVisibleTextRect
    //FIXME: consider use of getWordWrap() instead
    doc_rect.mRight = mScroller
        ? llmax(mVisibleTextRect.getWidth(), mTextBoundingRect.mRight)
        : mVisibleTextRect.getWidth();
    doc_rect.mTop = llmax(mVisibleTextRect.mTop, mTextBoundingRect.mTop);

    if (!mScroller)
    {
        // push doc rect to top of text widget
        switch(mVAlign)
        {
        case LLFontGL::TOP:
            doc_rect.translate(0, mVisibleTextRect.getHeight() - doc_rect.mTop);
            break;
        case LLFontGL::VCENTER:
            doc_rect.translate(0, (mVisibleTextRect.getHeight() - doc_rect.mTop) / 2);
        case LLFontGL::BOTTOM:
        default:
            break;
        }
    }

    mDocumentView->setShape(doc_rect);

    //update mVisibleTextRect *after* mDocumentView has been resized
    // so that scrollbars are added if document needs to scroll
    // since mVisibleTextRect does not include scrollbars
    mVisibleTextRect = mScroller ? mScroller->getContentWindowRect() : getLocalRect();
    //FIXME: replace border with image?
    if (mBorderVisible)
    {
        // <FS:Zi> Commented out to prevent contents from scrolling away while typing
        // mVisibleTextRect.stretch(-1);
        // </FS:Zi>
    }
    if (mVisibleTextRect != old_text_rect)
    {
        needsReflow();
    }

    // update mTextBoundingRect after mVisibleTextRect took scrolls into account
    if (!mLineInfoList.empty() && mScroller)
    {
        S32 delta_pos = 0;

        switch(mVAlign)
        {
        case LLFontGL::TOP:
            delta_pos = llmax(mVisibleTextRect.getHeight() - mTextBoundingRect.mTop, -mTextBoundingRect.mBottom);
            break;
        case LLFontGL::VCENTER:
            delta_pos = (llmax(mVisibleTextRect.getHeight() - mTextBoundingRect.mTop, -mTextBoundingRect.mBottom) + (mVisibleTextRect.mBottom - mTextBoundingRect.mBottom)) / 2;
            break;
        case LLFontGL::BOTTOM:
            delta_pos = mVisibleTextRect.mBottom - mTextBoundingRect.mBottom;
            break;
        case LLFontGL::BASELINE:
            // do nothing
            break;
        }
        // move line segments to fit new visible rect
        if (delta_pos != 0)
        {
            for (line_list_t::iterator it = mLineInfoList.begin(); it != mLineInfoList.end(); ++it)
            {
                it->mRect.translate(0, delta_pos);
            }
            mTextBoundingRect.translate(0, delta_pos);
        }
    }

    // update document container again, using new mVisibleTextRect (that has scrollbars enabled as needed)
    doc_rect.mBottom = llmin(mVisibleTextRect.mBottom,  mTextBoundingRect.mBottom);
    doc_rect.mLeft = 0;
    doc_rect.mRight = mScroller
        ? llmax(mVisibleTextRect.getWidth(), mTextBoundingRect.mRight)
        : mVisibleTextRect.getWidth();
    doc_rect.mTop = llmax(mVisibleTextRect.getHeight(), mTextBoundingRect.getHeight()) + doc_rect.mBottom;
    if (!mScroller)
    {
        // push doc rect to top of text widget
        switch(mVAlign)
        {
        case LLFontGL::TOP:
            doc_rect.translate(0, mVisibleTextRect.getHeight() - doc_rect.mTop);
            break;
        case LLFontGL::VCENTER:
            doc_rect.translate(0, (mVisibleTextRect.getHeight() - doc_rect.mTop) / 2);
        case LLFontGL::BOTTOM:
        default:
            break;
        }
    }
    mDocumentView->setShape(doc_rect);
}


void LLTextBase::startSelection()
{
    if( !mIsSelecting )
    {
        mIsSelecting = true;
        mSelectionStart = mCursorPos;
        mSelectionEnd = mCursorPos;
    }
}

void LLTextBase::endSelection()
{
    if( mIsSelecting )
    {
        mIsSelecting = false;
        mSelectionEnd = mCursorPos;
    }
}

// get portion of document that is visible in text editor
LLRect LLTextBase::getVisibleDocumentRect() const
{
    if (mScroller)
    {
        return mScroller->getVisibleContentRect();
    }
    else if (mClip)
    {
        LLRect visible_text_rect = getVisibleTextRect();
        LLRect doc_rect = mDocumentView->getRect();
        visible_text_rect.translate(-doc_rect.mLeft, -doc_rect.mBottom);

        // reject partially visible lines
        LLRect visible_lines_rect;
        for (line_list_t::const_iterator it = mLineInfoList.begin(), end_it = mLineInfoList.end();
            it != end_it;
            ++it)
        {
            bool line_visible = mClipPartial ? visible_text_rect.contains(it->mRect) : visible_text_rect.overlaps(it->mRect);
            if (line_visible)
            {
                if (visible_lines_rect.isEmpty())
                {
                    visible_lines_rect = it->mRect;
                }
                else
                {
                    visible_lines_rect.unionWith(it->mRect);
                }
            }
        }
        return visible_lines_rect;
    }
    else
    {   // entire document rect is visible
        // but offset according to height of widget

        LLRect doc_rect = mDocumentView->getLocalRect();
        doc_rect.mLeft -= mDocumentView->getRect().mLeft;
        // adjust for height of text above widget baseline
        doc_rect.mBottom = doc_rect.getHeight() - mVisibleTextRect.getHeight();
        return doc_rect;
    }
}

boost::signals2::connection LLTextBase::setURLClickedCallback(const commit_signal_t::slot_type& cb)
{
    if (!mURLClickSignal)
    {
        mURLClickSignal = new commit_signal_t();
    }
    return mURLClickSignal->connect(cb);
}

boost::signals2::connection LLTextBase::setIsFriendCallback(const is_friend_signal_t::slot_type& cb)
{
    if (!mIsFriendSignal)
    {
        mIsFriendSignal = new is_friend_signal_t();
    }
    return mIsFriendSignal->connect(cb);
}

boost::signals2::connection LLTextBase::setIsObjectBlockedCallback(const is_blocked_signal_t::slot_type& cb)
{
    if (!mIsObjectBlockedSignal)
    {
        mIsObjectBlockedSignal = new is_blocked_signal_t();
    }
    return mIsObjectBlockedSignal->connect(cb);
}

//
// LLTextSegment
//

LLTextSegment::~LLTextSegment()
{}

// static
LLStyleSP LLTextSegment::cloneStyle(LLTextBase& target, const LLStyle* source)
{
    // Take most params from target
    LLStyle::Params params = target.getStyleParams();
    LLStyle* style = new LLStyle(params);

    // Take some params from source
    style->setLinkHREF(source->getLinkHREF());
    if (source->isImage())
    {
        style->setImage(source->getImage()->getName());
    }

    return style;
}


bool LLTextSegment::getDimensionsF32(S32 first_char, S32 num_chars, F32& width, S32& height) const { width = 0; height = 0; return false; }
bool LLTextSegment::getDimensions(S32 first_char, S32 num_chars, S32& width, S32& height) const
{
    F32 fwidth = 0;
    bool result = getDimensionsF32(first_char, num_chars, fwidth, height);
    width = ll_round(fwidth);
    return result;
}

S32 LLTextSegment::getOffset(S32 segment_local_x_coord, S32 start_offset, S32 num_chars, bool round) const { return 0; }
S32 LLTextSegment::getNumChars(S32 num_pixels, S32 segment_offset, S32 line_offset, S32 max_chars, S32 line_ind) const { return 0; }
void LLTextSegment::updateLayout(const LLTextBase& editor) {}
F32 LLTextSegment::draw(S32 start, S32 end, S32 selection_start, S32 selection_end, const LLRectf& draw_rect) { return draw_rect.mLeft; }
bool LLTextSegment::canEdit() const { return false; }
void LLTextSegment::unlinkFromDocument(LLTextBase*) {}
void LLTextSegment::linkToDocument(LLTextBase*) {}
const LLUIColor& LLTextSegment::getColor() const { static const LLUIColor white = LLUIColorTable::instance().getColor("White", LLColor4::white); return white; }
//void LLTextSegment::setColor(const LLUIColor &color) {}
LLStyleConstSP LLTextSegment::getStyle() const {static LLStyleConstSP sp(new LLStyle()); return sp; }
void LLTextSegment::setStyle(LLStyleConstSP style) {}
void LLTextSegment::setToken( LLKeywordToken* token ) {}
LLKeywordToken* LLTextSegment::getToken() const { return NULL; }
void LLTextSegment::setToolTip( const std::string &msg ) {}
void LLTextSegment::dump() const {}
bool LLTextSegment::handleMouseDown(S32 x, S32 y, MASK mask) { return false; }
bool LLTextSegment::handleMouseUp(S32 x, S32 y, MASK mask) { return false; }
bool LLTextSegment::handleMiddleMouseDown(S32 x, S32 y, MASK mask) { return false; }
bool LLTextSegment::handleMiddleMouseUp(S32 x, S32 y, MASK mask) { return false; }
bool LLTextSegment::handleRightMouseDown(S32 x, S32 y, MASK mask) { return false; }
bool LLTextSegment::handleRightMouseUp(S32 x, S32 y, MASK mask) { return false; }
bool LLTextSegment::handleDoubleClick(S32 x, S32 y, MASK mask) { return false; }
bool LLTextSegment::handleHover(S32 x, S32 y, MASK mask) { return false; }
bool LLTextSegment::handleScrollWheel(S32 x, S32 y, S32 clicks) { return false; }
bool LLTextSegment::handleScrollHWheel(S32 x, S32 y, S32 clicks) { return false; }
bool LLTextSegment::handleToolTip(S32 x, S32 y, MASK mask) { return false; }
const std::string&  LLTextSegment::getName() const
{
    return LLStringUtil::null;
}
void LLTextSegment::onMouseCaptureLost() {}
void LLTextSegment::screenPointToLocal(S32 screen_x, S32 screen_y, S32* local_x, S32* local_y) const {}
void LLTextSegment::localPointToScreen(S32 local_x, S32 local_y, S32* screen_x, S32* screen_y) const {}
bool LLTextSegment::hasMouseCapture() { return false; }

//
// LLNormalTextSegment
//

LLNormalTextSegment::LLNormalTextSegment( LLStyleConstSP style, S32 start, S32 end, LLTextBase& editor )
:   LLTextSegment(start, end),
    mStyle( style ),
    mToken(NULL),
    mEditor(editor),
    mLastGeneration(-1)
{
    mFontHeight = mStyle->getFont()->getLineHeight();
    mCanEdit = !mStyle->getDrawHighlightBg();

    LLUIImagePtr image = mStyle->getImage();
    if (image.notNull())
    {
        mImageLoadedConnection = image->addLoadedCallback(boost::bind(&LLTextBase::needsReflow, &mEditor, start));
    }
}

LLNormalTextSegment::LLNormalTextSegment( const LLUIColor& color, S32 start, S32 end, LLTextBase& editor, bool is_visible)
:   LLTextSegment(start, end),
    mToken(NULL),
    mEditor(editor),
    mLastGeneration(-1)
{
    mStyle = new LLStyle(LLStyle::Params().visible(is_visible).color(color));

    mFontHeight = mStyle->getFont()->getLineHeight();
}

LLNormalTextSegment::~LLNormalTextSegment()
{
    mImageLoadedConnection.disconnect();
}


F32 LLNormalTextSegment::draw(S32 start, S32 end, S32 selection_start, S32 selection_end, const LLRectf& draw_rect)
{
    if( end - start > 0 )
    {
        return drawClippedSegment( getStart() + start, getStart() + end, selection_start, selection_end, draw_rect);
    }
    else
    {
        mFontBufferPreSelection.reset();
        mFontBufferSelection.reset();
        mFontBufferPostSelection.reset();
    }
    return draw_rect.mLeft;
}

// Draws a single text segment, reversing the color for selection if needed.
F32 LLNormalTextSegment::drawClippedSegment(S32 seg_start, S32 seg_end, S32 selection_start, S32 selection_end, LLRectf rect)
{
    F32 right_x = rect.mLeft;
    if (!mStyle->isVisible())
    {
        return right_x;
    }

    F32 alpha = LLViewDrawContext::getCurrentContext().mAlpha;

    const LLWString& text = getWText();
    S32 text_gen = mEditor.getTextGeneration();

    if (text_gen != mLastGeneration)
    {
        mLastGeneration = text_gen;

        mFontBufferPreSelection.reset();
        mFontBufferSelection.reset();
        mFontBufferPostSelection.reset();
    }

    const LLFontGL* font = mStyle->getFont();
    LLColor4 color = (mEditor.getReadOnly() ? mStyle->getReadOnlyColor() : mStyle->getColor())  % (alpha * mStyle->getAlpha());
    bool use_font_buffers = useFontBuffers();

    if( selection_start > seg_start )
    {
        // Draw normally
        S32 start = seg_start;
        S32 end = llmin( selection_start, seg_end );
        S32 length =  end - start;
        if (use_font_buffers)
        {
            mFontBufferPreSelection.render(
                font,
                text, start,
                rect,
                color,
                LLFontGL::LEFT, mEditor.mTextVAlign,
                LLFontGL::NORMAL,
                mStyle->getShadowType(),
                length,
                &right_x,
                mEditor.getUseEllipses(),
                mEditor.getUseColor());
        }
        else
        {
            // Font buffer doesn't do well with changes and huge notecard with a bunch
            // of segments will see a lot of buffer updates, so instead use derect
            // rendering to cache.
            // Todo: instead of mLastGeneration make buffer invalidation more fine grained
            // like string hash of a given segment.
            font->render(
                text, start,
                rect,
                color,
                LLFontGL::LEFT, mEditor.mTextVAlign,
                LLFontGL::NORMAL,
                mStyle->getShadowType(),
                length,
                &right_x,
                mEditor.getUseEllipses(),
                mEditor.getUseColor());
        }
    }
    rect.mLeft = right_x;

    if( (selection_start < seg_end) && (selection_end > seg_start) )
    {
        // Draw reversed
        S32 start = llmax( selection_start, seg_start );
        S32 end = llmin( selection_end, seg_end );
        S32 length = end - start;

        if (use_font_buffers)
        {
            mFontBufferSelection.render(
                font,
                text, start,
                rect,
                mStyle->getSelectedColor().get(),
                LLFontGL::LEFT, mEditor.mTextVAlign,
                LLFontGL::NORMAL,
                LLFontGL::NO_SHADOW,
                length,
                &right_x,
                mEditor.getUseEllipses(),
                mEditor.getUseColor());
        }
        else
        {
            font->render(
                text, start,
                rect,
                mStyle->getSelectedColor().get(),
                LLFontGL::LEFT, mEditor.mTextVAlign,
                LLFontGL::NORMAL,
                LLFontGL::NO_SHADOW,
                length,
                &right_x,
                mEditor.getUseEllipses(),
                mEditor.getUseColor());
        }
    }
    rect.mLeft = right_x;
    if( selection_end < seg_end )
    {
        // Draw normally
        S32 start = llmax( selection_end, seg_start );
        S32 end = seg_end;
        S32 length = end - start;
        if (use_font_buffers)
        {
            mFontBufferPostSelection.render(
                font,
                text, start,
                rect,
                color,
                LLFontGL::LEFT, mEditor.mTextVAlign,
                LLFontGL::NORMAL,
                mStyle->getShadowType(),
                length,
                &right_x,
                mEditor.getUseEllipses(),
                mEditor.getUseColor());
        }
        else
        {
            font->render(
                text, start,
                rect,
                color,
                LLFontGL::LEFT, mEditor.mTextVAlign,
                LLFontGL::NORMAL,
                mStyle->getShadowType(),
                length,
                &right_x,
                mEditor.getUseEllipses(),
                mEditor.getUseColor());

        }
    }
    return right_x;
}

bool LLNormalTextSegment::handleHover(S32 x, S32 y, MASK mask)
{
    if (getStyle() && getStyle()->isLink())
    {
        // Only process the click if it's actually in this segment, not to the right of the end-of-line.
        if(mEditor.getSegmentAtLocalPos(x, y, false) == this)
        {
            LLUI::getInstance()->getWindow()->setCursor(UI_CURSOR_HAND);
            return true;
        }
    }
    return false;
}

bool LLNormalTextSegment::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
    if (getStyle() && getStyle()->isLink())
    {
        // Only process the click if it's actually in this segment, not to the right of the end-of-line.
        if(mEditor.getSegmentAtLocalPos(x, y, false) == this)
        {
            mEditor.createUrlContextMenu(x, y, getStyle()->getLinkHREF());
            return true;
        }
    }
    return false;
}

bool LLNormalTextSegment::handleMouseDown(S32 x, S32 y, MASK mask)
{
    if (getStyle() && getStyle()->isLink())
    {
        // Only process the click if it's actually in this segment, not to the right of the end-of-line.
        if(mEditor.getSegmentAtLocalPos(x, y, false) == this)
        {
            // eat mouse down event on hyperlinks, so we get the mouse up
            return true;
        }
    }

    return false;
}

bool LLNormalTextSegment::handleMouseUp(S32 x, S32 y, MASK mask)
{
    if (getStyle() && getStyle()->isLink())
    {
        // Only process the click if it's actually in this segment, not to the right of the end-of-line.
        if(mEditor.getSegmentAtLocalPos(x, y, false) == this)
        {
            std::string url = getStyle()->getLinkHREF();
            if (!mEditor.mForceUrlsExternal)
            {
                LLUrlAction::clickAction(url, mEditor.isContentTrusted());
            }
            else if (!LLUrlAction::executeSLURL(url, mEditor.isContentTrusted()))
            {
                LLUrlAction::openURLExternal(url);
            }
            return true;
        }
    }

    return false;
}

bool LLNormalTextSegment::handleToolTip(S32 x, S32 y, MASK mask)
{
    std::string msg;
    // do we have a tooltip for a loaded keyword (for script editor)?
    if (mToken && !mToken->getToolTip().empty())
    {
        const LLWString& wmsg = mToken->getToolTip();
        LLToolTipMgr::instance().show(wstring_to_utf8str(wmsg), (mToken->getType() == LLKeywordToken::TT_FUNCTION));
        return true;
    }
    // or do we have an explicitly set tooltip (e.g., for Urls)
    if (!mTooltip.empty())
    {
        LLToolTipMgr::instance().show(mTooltip);
        return true;
    }

    return false;
}

void LLNormalTextSegment::setToolTip(const std::string& tooltip)
{
    // we cannot replace a keyword tooltip that's loaded from a file
    if (mToken)
    {
        LL_WARNS() << "LLTextSegment::setToolTip: cannot replace keyword tooltip." << LL_ENDL;
        return;
    }
    mTooltip = tooltip;
}

// virtual
LLTextSegmentPtr LLNormalTextSegment::clone(LLTextBase& target) const
{
    LLStyleConstSP sp(cloneStyle(target, mStyle));
    return new LLNormalTextSegment(sp, mStart, mEnd, target);
}

bool LLNormalTextSegment::getDimensionsF32(S32 first_char, S32 num_chars, F32& width, S32& height) const
{
    height = 0;
    width = 0;
    if (num_chars > 0)
    {
        height = mFontHeight;
        const LLWString &text = getWText();
        // if last character is a newline, then return true, forcing line break
        width = mStyle->getFont()->getWidthF32(text.c_str(), mStart + first_char, num_chars, true);
    }
    return false;
}

S32 LLNormalTextSegment::getOffset(S32 segment_local_x_coord, S32 start_offset, S32 num_chars, bool round) const
{
    const LLWString &text = getWText();
    return mStyle->getFont()->charFromPixelOffset(text.c_str(), mStart + start_offset,
                                               (F32)segment_local_x_coord,
                                               F32_MAX,
                                               num_chars,
                                               round);
}

S32 LLNormalTextSegment::getNumChars(S32 num_pixels, S32 segment_offset, S32 line_offset, S32 max_chars, S32 line_ind) const
{
    const LLWString &text = getWText();

    LLUIImagePtr image = mStyle->getImage();
    if( image.notNull())
    {
        num_pixels = llmax(0, num_pixels - image->getWidth());
    }

    S32 last_char = mEnd;

    // <FS:Ansariel> Prevent unnecessary calculations
    S32 start_offset = mStart + segment_offset;

    // set max characters to length of segment, or to first newline
    // <FS:Ansariel> Prevent unnecessary calculations
    //max_chars = llmin(max_chars, last_char - (mStart + segment_offset));
    max_chars = llmin(max_chars, last_char - start_offset);

    // if no character yet displayed on this line, don't require word wrapping since
    // we can just move to the next line, otherwise insist on it so we make forward progress
    LLFontGL::EWordWrapStyle word_wrap_style = (line_offset == 0)
        ? LLFontGL::WORD_BOUNDARY_IF_POSSIBLE
        : LLFontGL::ONLY_WORD_BOUNDARIES;


    // <FS:Ansariel> Prevent unnecessary calculations

    //S32 offsetLength = static_cast<S32>(text.length()) - (segment_offset + mStart);
    S32 offsetLength = static_cast<S32>(text.length()) - start_offset;

    //if(getLength() < segment_offset + mStart)
    if(getLength() < start_offset)
    {
        LL_INFOS() << "getLength() < segment_offset + mStart\t getLength()\t" << getLength() << "\tsegment_offset:\t"
                        << segment_offset << "\tmStart:\t" << mStart << "\tsegments\t" << mEditor.mSegments.size() << "\tmax_chars\t" << max_chars << LL_ENDL;
    }

    if( (offsetLength + 1) < max_chars)
    {
        LL_INFOS() << "offsetString.length() + 1 < max_chars\t max_chars:\t" << max_chars << "\toffsetString.length():\t" << offsetLength << " getLength() : "
            << getLength() << "\tsegment_offset:\t" << segment_offset << "\tmStart:\t" << mStart << "\tsegments\t" << mEditor.mSegments.size() << LL_ENDL;
    }

    // <FS:Ansariel> Prevent unnecessary calculations
    //S32 num_chars = mStyle->getFont()->maxDrawableChars( text.c_str() + (segment_offset + mStart),
    S32 num_chars = mStyle->getFont()->maxDrawableChars(text.c_str() + start_offset,
                                                (F32)num_pixels,
                                                max_chars,
                                                word_wrap_style);

    if (num_chars == 0
        && line_offset == 0
        && max_chars > 0)
    {
        // If at the beginning of a line, and a single character won't fit, draw it anyway
        num_chars = 1;
    }

    // include *either* the EOF or newline character in this run of text
    // but not both
    // <FS:Ansariel> Prevent unnecessary calculations
    //S32 last_char_in_run = mStart + segment_offset + num_chars;
    S32 last_char_in_run = start_offset + num_chars;
    // check length first to avoid indexing off end of string
    if (last_char_in_run < mEnd
        && (last_char_in_run >= getLength()))
    {
        num_chars++;
    }
    return num_chars;
}

void LLNormalTextSegment::updateLayout(const class LLTextBase& editor)
{
    LLTextSegment::updateLayout(editor);

    mFontBufferPreSelection.reset();
    mFontBufferSelection.reset();
    mFontBufferPostSelection.reset();
}

void LLNormalTextSegment::dump() const
{
    LL_INFOS() << "Segment [" <<
//          mColor.mV[VRED] << ", " <<
//          mColor.mV[VGREEN] << ", " <<
//          mColor.mV[VBLUE] << "]\t[" <<
        mStart << ", " <<
        getEnd() << "]" <<
        LL_ENDL;
}

/*virtual*/
const LLWString& LLNormalTextSegment::getWText()    const
{
    return mEditor.getWText();
}

/*virtual*/
const S32 LLNormalTextSegment::getLength() const
{
    return mEditor.getLength();
}

LLLabelTextSegment::LLLabelTextSegment( LLStyleConstSP style, S32 start, S32 end, LLTextBase& editor )
:   LLNormalTextSegment(style, start, end, editor)
{
}

LLLabelTextSegment::LLLabelTextSegment( const LLUIColor& color, S32 start, S32 end, LLTextBase& editor, bool is_visible)
:   LLNormalTextSegment(color, start, end, editor, is_visible)
{
}

// virtual
LLTextSegmentPtr LLLabelTextSegment::clone(LLTextBase& target) const
{
    LLStyleConstSP sp(cloneStyle(target, mStyle));
    return new LLLabelTextSegment(sp, mStart, mEnd, target);
}

/*virtual*/
const LLWString& LLLabelTextSegment::getWText() const
{
    return mEditor.getWlabel();
}
/*virtual*/
const S32 LLLabelTextSegment::getLength() const
{
    return static_cast<S32>(mEditor.getWlabel().length());
}

//
// LLEmojiTextSegment
//
LLEmojiTextSegment::LLEmojiTextSegment(LLStyleConstSP style, S32 start, S32 end, LLTextBase& editor)
    : LLNormalTextSegment(style, start, end, editor)
{
}

LLEmojiTextSegment::LLEmojiTextSegment(const LLUIColor& color, S32 start, S32 end, LLTextBase& editor, bool is_visible)
    : LLNormalTextSegment(color, start, end, editor, is_visible)
{
}

// virtual
LLTextSegmentPtr LLEmojiTextSegment::clone(LLTextBase& target) const
{
    LLStyleConstSP sp(cloneStyle(target, mStyle));
    return new LLEmojiTextSegment(sp, mStart, mEnd, target);
}

bool LLEmojiTextSegment::handleToolTip(S32 x, S32 y, MASK mask)
{
    if (mTooltip.empty())
    {
        LLWString emoji = getWText().substr(getStart(), getEnd() - getStart());
        if (!emoji.empty())
        {
            mTooltip = LLEmojiHelper::instance().getToolTip(emoji[0]);
        }
    }

    return LLNormalTextSegment::handleToolTip(x, y, mask);
}

//
// LLOnHoverChangeableTextSegment
//

LLOnHoverChangeableTextSegment::LLOnHoverChangeableTextSegment( LLStyleConstSP style, LLStyleConstSP normal_style, S32 start, S32 end, LLTextBase& editor ):
      LLNormalTextSegment(normal_style, start, end, editor),
      mHoveredStyle(style),
      mNormalStyle(normal_style){}

// virtual
LLTextSegmentPtr LLOnHoverChangeableTextSegment::clone(LLTextBase& target) const
{
    LLStyleConstSP hsp(cloneStyle(target, mHoveredStyle));
    LLStyleConstSP nsp(cloneStyle(target, mNormalStyle));
    return new LLOnHoverChangeableTextSegment(hsp, nsp, mStart, mEnd, target);
}

/*virtual*/
F32 LLOnHoverChangeableTextSegment::draw(S32 start, S32 end, S32 selection_start, S32 selection_end, const LLRectf& draw_rect)
{
    F32 result = LLNormalTextSegment::draw(start, end, selection_start, selection_end, draw_rect);
    if (end == mEnd - mStart)
    {
        mStyle = mNormalStyle;
    }
    return result;
}

/*virtual*/
bool LLOnHoverChangeableTextSegment::handleHover(S32 x, S32 y, MASK mask)
{
    mStyle = mEditor.getSkipLinkUnderline() ? mNormalStyle : mHoveredStyle;
    return LLNormalTextSegment::handleHover(x, y, mask);
}


//
// LLInlineViewSegment
//

LLInlineViewSegment::LLInlineViewSegment(const Params& p, S32 start, S32 end)
:   LLTextSegment(start, end),
    mView(p.view),
    mForceNewLine(p.force_newline),
    mLeftPad(p.left_pad),
    mRightPad(p.right_pad),
    mTopPad(p.top_pad),
    mBottomPad(p.bottom_pad)
{
}

LLInlineViewSegment::~LLInlineViewSegment()
{
    mView->die();
}

// virtual
LLTextSegmentPtr LLInlineViewSegment::clone(LLTextBase& target) const
{
    llassert_always_msg(false, "NOT SUPPORTED");
    return nullptr;
}

bool LLInlineViewSegment::getDimensionsF32(S32 first_char, S32 num_chars, F32& width, S32& height) const
{
    if (first_char == 0 && num_chars == 0)
    {
        // We didn't fit on a line or were forced to new string
        // the widget will fall on the next line, so width here is 0
        width = 0;

        if (mForceNewLine)
        {
            // Chat, string can't be smaller then font height even if it is empty
            height = LLStyle::getDefaultFont()->getLineHeight();

            return true; // new line
        }
        else
        {
            // height from previous segment in same string will be used, word-wrap
            height = 0;
        }

    }
    else
    {
        width = (F32)(mLeftPad + mRightPad + mView->getRect().getWidth());
        height = mBottomPad + mTopPad + mView->getRect().getHeight();
    }

    return false;
}

S32 LLInlineViewSegment::getNumChars(S32 num_pixels, S32 segment_offset, S32 line_offset, S32 max_chars, S32 line_ind) const
{
    // if putting a widget anywhere but at the beginning of a line
    // and the widget doesn't fit or mForceNewLine is true
    // then return 0 chars for that line, and all characters for the next
    if (mForceNewLine && line_ind == 0)
    {
        return 0;
    }
    else if (line_offset != 0 && num_pixels < mView->getRect().getWidth())
    {
        return 0;
    }
    else
    {
        return mEnd - mStart;
    }
}

void LLInlineViewSegment::updateLayout(const LLTextBase& editor)
{
    LLRect start_rect = editor.getDocRectFromDocIndex(mStart);
    mView->setOrigin(start_rect.mLeft + mLeftPad, start_rect.mBottom + mBottomPad);
}

F32 LLInlineViewSegment::draw(S32 start, S32 end, S32 selection_start, S32 selection_end, const LLRectf& draw_rect)
{
    // return padded width of widget
    // widget is actually drawn during mDocumentView's draw()
    return (F32)(draw_rect.mLeft + mView->getRect().getWidth() + mLeftPad + mRightPad);
}

void LLInlineViewSegment::unlinkFromDocument(LLTextBase* editor)
{
    editor->removeDocumentChild(mView);
}

void LLInlineViewSegment::linkToDocument(LLTextBase* editor)
{
    editor->addDocumentChild(mView);
}

LLLineBreakTextSegment::LLLineBreakTextSegment(S32 pos):LLTextSegment(pos,pos+1)
{
    mFontHeight = LLStyle::getDefaultFont()->getLineHeight();
}
LLLineBreakTextSegment::LLLineBreakTextSegment(LLStyleConstSP style,S32 pos):LLTextSegment(pos,pos+1)
{
    mFontHeight = style->getFont()->getLineHeight();
}
LLLineBreakTextSegment::~LLLineBreakTextSegment()
{
}

// virtual
LLTextSegmentPtr LLLineBreakTextSegment::clone(LLTextBase& target) const
{
    LLLineBreakTextSegment* copy = new LLLineBreakTextSegment(mStart);
    copy->mFontHeight = mFontHeight;
    return copy;
}
bool LLLineBreakTextSegment::getDimensionsF32(S32 first_char, S32 num_chars, F32& width, S32& height) const
{
    width = 0;
    height = mFontHeight;

    return true;
}
S32 LLLineBreakTextSegment::getNumChars(S32 num_pixels, S32 segment_offset, S32 line_offset, S32 max_chars, S32 line_ind) const
{
    return 1;
}
F32 LLLineBreakTextSegment::draw(S32 start, S32 end, S32 selection_start, S32 selection_end, const LLRectf& draw_rect)
{
    return  draw_rect.mLeft;
}

LLImageTextSegment::LLImageTextSegment(LLStyleConstSP style,S32 pos,class LLTextBase& editor)
:   LLTextSegment(pos,pos+1),
    mStyle( style ),
    mEditor(editor)
{
}

LLImageTextSegment::~LLImageTextSegment()
{
}

// virtual
LLTextSegmentPtr LLImageTextSegment::clone(LLTextBase& target) const
{
    LLStyleConstSP sp(cloneStyle(target, mStyle));
    return new LLImageTextSegment(sp, mStart, target);
}

static const S32 IMAGE_HPAD = 3;

// virtual
bool LLImageTextSegment::getDimensionsF32(S32 first_char, S32 num_chars, F32& width, S32& height) const
{
    width = 0;
    height = mStyle->getFont()->getLineHeight();

    LLUIImagePtr image = mStyle->getImage();
    if( num_chars>0 && image.notNull())
    {
        width += image->getWidth() + IMAGE_HPAD;
        height = llmax(height, image->getHeight() + IMAGE_HPAD );
    }
    return false;
}

S32  LLImageTextSegment::getNumChars(S32 num_pixels, S32 segment_offset, S32 line_offset, S32 max_chars, S32 line_ind) const
{
    LLUIImagePtr image = mStyle->getImage();

    if (image.isNull())
    {
        return 1;
    }

    S32 image_width = image->getWidth();
    if(line_offset == 0 || num_pixels>image_width + IMAGE_HPAD)
    {
        return 1;
    }

    return 0;
}

bool LLImageTextSegment::handleToolTip(S32 x, S32 y, MASK mask)
{
    if (!mTooltip.empty())
    {
        LLToolTipMgr::instance().show(mTooltip);
        return true;
    }

    return false;
}

void LLImageTextSegment::setToolTip(const std::string& tooltip)
{
    mTooltip = tooltip;
}

F32 LLImageTextSegment::draw(S32 start, S32 end, S32 selection_start, S32 selection_end, const LLRectf& draw_rect)
{
    if ( (start >= 0) && (end <= mEnd - mStart))
    {
        LLColor4 color = LLColor4::white % mEditor.getDrawContext().mAlpha;
        LLUIImagePtr image = mStyle->getImage();
        if (image.notNull())
        {
            S32 style_image_height = image->getHeight();
            S32 style_image_width = image->getWidth();
            // Text is drawn from the top of the draw_rect downward

            S32 text_center = (S32)(draw_rect.mTop - (draw_rect.getHeight() / 2.f));
            // Align image to center of draw rect
            S32 image_bottom = text_center - (style_image_height / 2);
            image->draw((S32)draw_rect.mLeft, image_bottom,
                style_image_width, style_image_height, color);

            const S32 IMAGE_HPAD = 3;
            return draw_rect.mLeft + style_image_width + IMAGE_HPAD;
        }
    }
    return 0.0;
}

void LLTextBase::setWordWrap(bool wrap)
{
    mWordWrap = wrap;
}
