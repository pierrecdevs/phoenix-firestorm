/**
 * @file postDeferredNoDoFF.glsl
 *
 * $LicenseInfo:firstyear=2007&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2007, Linden Research, Inc.
 * Copyright (C) 2025, William Weaver (paperwork) @ Second Life
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

/*[EXTRA_CODE_HERE]*/

out vec4 frag_color;

uniform sampler2D diffuseRect;
uniform sampler2D depthMap;

uniform vec2 screen_res;
in vec2 vary_fragcoord;

// <AP:WW> ADD START: Declare Saturation Uniform
uniform float ap_saturation; // 0=gray, 1=normal, >1=more saturated
// <AP:WW> ADD END: Declare Saturation Uniform
// <AP:WW> ADD START: Declare Luminance Weights Uniform
uniform vec3 ap_luminance_weights; // User-defined RGB weights
// <AP:WW> ADD END: Declare Luminance Weights Uniform

//=================================
// borrowed noise from:
//  <https://www.shadertoy.com/view/4dS3Wd>
//  By Morgan McGuire @morgan3d, http://graphicscodex.com
//
float hash(float n) { return fract(sin(n) * 1e4); }
float hash(vec2 p) { return fract(1e4 * sin(17.0 * p.x + p.y * 0.1) * (0.1 + abs(sin(p.y * 13.0 + p.x)))); }

float noise(float x) {
    float i = floor(x);
    float f = fract(x);
    float u = f * f * (3.0 - 2.0 * f);
    return mix(hash(i), hash(i + 1.0), u);
}

float noise(vec2 x) {
    vec2 i = floor(x);
    vec2 f = fract(x);

    // Four corners in 2D of a tile
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));

    // Simple 2D lerp using smoothstep envelope between the values.
    // return vec3(mix(mix(a, b, smoothstep(0.0, 1.0, f.x)),
    //          mix(c, d, smoothstep(0.0, 1.0, f.x)),
    //          smoothstep(0.0, 1.0, f.y)));

    // Same code, with the clamps in smoothstep and common subexpressions
    // optimized away.
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}

//=============================




void main()
{
    // Get the original color
    vec4 diff = texture(diffuseRect, vary_fragcoord.xy);

#ifdef HAS_NOISE
    // --- NOISE PATH ---
    // Calculate and add noise if enabled
    vec2 tc = vary_fragcoord.xy * screen_res * 4.0;
    vec3 seed = (diff.rgb + vec3(1.0)) * vec3(tc.xy, tc.x + tc.y);
    vec3 nz = vec3(noise(seed.rg), noise(seed.gb), noise(seed.rb));
    diff.rgb += nz * 0.003;

    // Calculate weights, luminance, and apply saturation INSIDE noise block
    vec3 weights = ap_luminance_weights / max(dot(ap_luminance_weights, vec3(1.0)), 0.0001);
    float luminance = dot(diff.rgb, weights);
    vec3 gray_version = vec3(luminance); // Renamed from gray_noise, fixed source
    diff.rgb = mix(gray_version, diff.rgb, ap_saturation);
    // diff.rgb is now potentially noisy AND saturation-adjusted

#else
    // --- NON-NOISE PATH ---
    // Calculate weights, luminance, and apply saturation OUTSIDE noise block
    vec3 weights = ap_luminance_weights / max(dot(ap_luminance_weights, vec3(1.0)), 0.0001);
    float luminance = dot(diff.rgb, weights);
    vec3 gray_version = vec3(luminance); // Renamed from gray_nonoise for consistency
    diff.rgb = mix(gray_version, diff.rgb, ap_saturation);
    // diff.rgb is now saturation-adjusted (but not noisy)

#endif // End HAS_NOISE

    // Clamp the final color regardless of the path taken
    diff.rgb = clamp(diff.rgb, 0.0, 1.0);

    // Set the final fragment color.
    frag_color = diff;

    // Set the fragment depth from the depth map.
    gl_FragDepth = texture(depthMap, vary_fragcoord.xy).r;
}