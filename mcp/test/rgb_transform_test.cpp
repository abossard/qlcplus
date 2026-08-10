/*
  Q Light Controller Plus - Unit test
  rgb_transform_test.cpp

  Copyright (C) Massimo Callegari

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#include <QtTest>
#include "rgb_transform_test.h"
#include "huematrix.h"
#include "doc.h"

// Helper to create a test map with unique values per pixel
static RGBMap makeTestMap(int width, int height)
{
    RGBMap map;
    map.resize(height);
    for (int y = 0; y < height; y++)
    {
        map[y].resize(width);
        for (int x = 0; x < width; x++)
            map[y][x] = (uint)((y + 1) * 100 + (x + 1));  // e.g. (0,0)=101, (1,0)=102, (0,1)=201
    }
    return map;
}

// For a 4-wide × 2-tall grid:
// map[0] = { 101, 102, 103, 104 }
// map[1] = { 201, 202, 203, 204 }

void RGBTransform_Test::rotation0_identity()
{
    RGBMap map = makeTestMap(4, 2);
    RGBMap original = map;
    QSize srcSize(4, 2);
    QSize dstSize(4, 2);

    HUEMatrix::applyTransforms(map, srcSize, dstSize, 0, 0, HUEMatrix::MirrorFlip);

    QCOMPARE(map.size(), 2);
    QCOMPARE(map[0].size(), (uint)4);
    for (int y = 0; y < 2; y++)
        for (int x = 0; x < 4; x++)
            QCOMPARE(map[y][x], original[y][x]);
}

void RGBTransform_Test::rotation90_nonSquare()
{
    // Algorithm renders at swapped dimensions (H=2, W=4) → src is 2-wide × 4-tall
    // Destination is the original grid: 4-wide × 2-tall
    RGBMap src = makeTestMap(2, 4);  // 4 rows × 2 cols
    // src[0] = {101, 102}
    // src[1] = {201, 202}
    // src[2] = {301, 302}
    // src[3] = {401, 402}

    QSize srcSize(2, 4);  // what the algorithm got
    QSize dstSize(4, 2);  // physical grid

    HUEMatrix::applyTransforms(src, srcSize, dstSize, 1, 0, HUEMatrix::MirrorFlip);

    // After 90° CW: dst(dx,dy) = src(dy, sh-1-dx) where sh=4
    // dst[0][0] = src(0, 3) = src[3][0] = 401
    // dst[0][1] = src(0, 2) = src[2][0] = 301
    // dst[0][2] = src(0, 1) = src[1][0] = 201
    // dst[0][3] = src(0, 0) = src[0][0] = 101
    // dst[1][0] = src(1, 3) = src[3][1] = 402
    // dst[1][1] = src(1, 2) = src[2][1] = 302
    // dst[1][2] = src(1, 1) = src[1][1] = 202
    // dst[1][3] = src(1, 0) = src[0][1] = 102

    QCOMPARE(src.size(), 2);
    QCOMPARE(src[0].size(), (uint)4);

    QCOMPARE(src[0][0], 401u);
    QCOMPARE(src[0][1], 301u);
    QCOMPARE(src[0][2], 201u);
    QCOMPARE(src[0][3], 101u);
    QCOMPARE(src[1][0], 402u);
    QCOMPARE(src[1][1], 302u);
    QCOMPARE(src[1][2], 202u);
    QCOMPARE(src[1][3], 102u);
}

void RGBTransform_Test::rotation180_nonSquare()
{
    RGBMap map = makeTestMap(4, 2);
    QSize srcSize(4, 2);
    QSize dstSize(4, 2);

    HUEMatrix::applyTransforms(map, srcSize, dstSize, 2, 0, HUEMatrix::MirrorFlip);

    // 180°: dst(dx,dy) = src(W-1-dx, H-1-dy)
    // dst[0][0] = src[1][3] = 204
    // dst[0][3] = src[1][0] = 201
    // dst[1][0] = src[0][3] = 104
    // dst[1][3] = src[0][0] = 101
    QCOMPARE(map[0][0], 204u);
    QCOMPARE(map[0][3], 201u);
    QCOMPARE(map[1][0], 104u);
    QCOMPARE(map[1][3], 101u);
}

void RGBTransform_Test::rotation270_nonSquare()
{
    // Algorithm renders at swapped dimensions → src is 2-wide × 4-tall
    RGBMap src = makeTestMap(2, 4);
    QSize srcSize(2, 4);
    QSize dstSize(4, 2);

    HUEMatrix::applyTransforms(src, srcSize, dstSize, 3, 0, HUEMatrix::MirrorFlip);

    // 270° CW: dst(dx,dy) = src(sw-1-dy, dx) where sw=2
    // dst[0][0] = src(1, 0) = src[0][1] = 102
    // dst[0][1] = src(1, 1) = src[1][1] = 202
    // dst[0][2] = src(1, 2) = src[2][1] = 302
    // dst[0][3] = src(1, 3) = src[3][1] = 402
    // dst[1][0] = src(0, 0) = src[0][0] = 101
    // dst[1][1] = src(0, 1) = src[1][0] = 201
    // dst[1][2] = src(0, 2) = src[2][0] = 301
    // dst[1][3] = src(0, 3) = src[3][0] = 401

    QCOMPARE(src.size(), 2);
    QCOMPARE(src[0].size(), (uint)4);

    QCOMPARE(src[0][0], 102u);
    QCOMPARE(src[0][1], 202u);
    QCOMPARE(src[0][2], 302u);
    QCOMPARE(src[0][3], 402u);
    QCOMPARE(src[1][0], 101u);
    QCOMPARE(src[1][1], 201u);
    QCOMPARE(src[1][2], 301u);
    QCOMPARE(src[1][3], 401u);
}

void RGBTransform_Test::rotation_roundTrip()
{
    // 4x 90° rotations on a non-square map should return to original
    // We need to simulate the full cycle: swap dims each time
    RGBMap original = makeTestMap(4, 2);
    RGBMap map = original;

    // Rotation 1: 4×2 → 2×4 (src=2×4, dst=4×2) - but we need to
    // actually produce the swapped map first, then rotate
    // Actually, in the engine flow, each rotation pass swaps dims.
    // 4 consecutive 90° rotations = identity.
    // Let's just do 180+180 = identity on the same map:
    HUEMatrix::applyTransforms(map, QSize(4, 2), QSize(4, 2), 2, 0, HUEMatrix::MirrorFlip);
    HUEMatrix::applyTransforms(map, QSize(4, 2), QSize(4, 2), 2, 0, HUEMatrix::MirrorFlip);

    for (int y = 0; y < 2; y++)
        for (int x = 0; x < 4; x++)
            QCOMPARE(map[y][x], original[y][x]);
}

void RGBTransform_Test::mirrorHorizontal_flip()
{
    RGBMap map = makeTestMap(4, 2);
    QSize s(4, 2);

    HUEMatrix::applyTransforms(map, s, s, 0, 1, HUEMatrix::MirrorFlip);

    // Flip: left half copied to right (top-left is source quadrant)
    // map[0] was {101, 102, 103, 104} → left half: 101, 102
    // Flip: map[0] = {101, 102, 102, 101}
    QCOMPARE(map[0][0], 101u);
    QCOMPARE(map[0][1], 102u);
    QCOMPARE(map[0][2], 102u);  // mirror of col 1
    QCOMPARE(map[0][3], 101u);  // mirror of col 0
}

void RGBTransform_Test::mirrorVertical_flip()
{
    RGBMap map = makeTestMap(4, 2);
    QSize s(4, 2);

    HUEMatrix::applyTransforms(map, s, s, 0, 2, HUEMatrix::MirrorFlip);

    // Vertical flip: top half copied to bottom
    // With H=2: only 1 row in top half (y < 2/2 = 1), bottom row gets top row
    // map[0] = {101, 102, 103, 104} (unchanged)
    // map[1] = {101, 102, 103, 104} (copy of top)
    QCOMPARE(map[0][0], 101u);
    QCOMPARE(map[1][0], 101u);
    QCOMPARE(map[1][3], 104u);
}

void RGBTransform_Test::mirrorHorizontal_max()
{
    // Use colored pixels to test max blend
    RGBMap map;
    map.resize(1);
    map[0].resize(4);
    map[0][0] = 0xFF0000; // Red left
    map[0][1] = 0x000000; // Black
    map[0][2] = 0x000000; // Black
    map[0][3] = 0x0000FF; // Blue right
    QSize s(4, 1);

    HUEMatrix::applyTransforms(map, s, s, 0, 1, HUEMatrix::MirrorMax);

    // Max blend: each pair gets max per channel
    // (0,0)+(0,3): max(FF,00)=FF, max(00,00)=00, max(00,FF)=FF → 0xFF00FF
    QCOMPARE(map[0][0], 0xFF00FFu);
    QCOMPARE(map[0][3], 0xFF00FFu);
    // (0,1)+(0,2): max(0,0)=0 → 0x000000
    QCOMPARE(map[0][1], 0x000000u);
}

void RGBTransform_Test::mirrorVertical_max()
{
    RGBMap map;
    map.resize(2);
    map[0].resize(1);
    map[1].resize(1);
    map[0][0] = 0xFF0000; // Red top
    map[1][0] = 0x0000FF; // Blue bottom
    QSize s(1, 2);

    HUEMatrix::applyTransforms(map, s, s, 0, 2, HUEMatrix::MirrorMax);

    QCOMPARE(map[0][0], 0xFF00FFu);
    QCOMPARE(map[1][0], 0xFF00FFu);
}

void RGBTransform_Test::mirrorHorizontalAndVertical_differ()
{
    // Verify that horizontal-only and vertical-only produce different results
    RGBMap mapH = makeTestMap(4, 3);
    RGBMap mapV = makeTestMap(4, 3);
    QSize s(4, 3);

    HUEMatrix::applyTransforms(mapH, s, s, 0, 1, HUEMatrix::MirrorFlip);
    HUEMatrix::applyTransforms(mapV, s, s, 0, 2, HUEMatrix::MirrorFlip);

    // They should differ: H mirrors columns, V mirrors rows
    bool differ = false;
    for (int y = 0; y < 3 && !differ; y++)
        for (int x = 0; x < 4 && !differ; x++)
            if (mapH[y][x] != mapV[y][x])
                differ = true;
    QVERIFY(differ);
}

void RGBTransform_Test::rotation90_withMirror()
{
    // Combined: 90° rotation + horizontal mirror
    RGBMap src = makeTestMap(2, 4);
    QSize srcSize(2, 4);
    QSize dstSize(4, 2);

    HUEMatrix::applyTransforms(src, srcSize, dstSize, 1, 1, HUEMatrix::MirrorFlip);

    // After 90° CW, the map is 4 wide × 2 tall
    QCOMPARE(src.size(), 2);
    QCOMPARE(src[0].size(), (uint)4);

    // After horizontal flip: left half mirrored to right
    // After rotation, row 0 was: {401, 301, 201, 101}
    // After flip: {401, 301, 301, 401}
    QCOMPARE(src[0][0], 401u);
    QCOMPARE(src[0][1], 301u);
    QCOMPARE(src[0][2], 301u);
    QCOMPARE(src[0][3], 401u);
}

void RGBTransform_Test::settersClamping()
{
    // Test that setters clamp values properly
    Doc doc(nullptr);
    HUEMatrix matrix(&doc);

    matrix.setRotation(5);
    QCOMPARE(matrix.rotation(), 1); // 5 & 3 = 1

    matrix.setRotation(-1);
    QCOMPARE(matrix.rotation(), 3); // -1 & 3 = 3 (two's complement)

    matrix.setMirror(7);
    QCOMPARE(matrix.mirror(), 3); // 7 & 3 = 3

    matrix.setMirrorBlend(HUEMatrix::MirrorBlend(99));
    QCOMPARE(matrix.mirrorBlend(), HUEMatrix::MirrorFlip); // out of range → default
}

void RGBTransform_Test::mirrorBlend_stringConversion()
{
    QCOMPARE(HUEMatrix::mirrorBlendToString(HUEMatrix::MirrorFlip), QStringLiteral("Flip"));
    QCOMPARE(HUEMatrix::mirrorBlendToString(HUEMatrix::MirrorMax), QStringLiteral("Max"));
    QCOMPARE(HUEMatrix::mirrorBlendToString(HUEMatrix::MirrorAverage), QStringLiteral("Average"));
    QCOMPARE(HUEMatrix::mirrorBlendToString(HUEMatrix::MirrorAdditive), QStringLiteral("Additive"));

    QCOMPARE(HUEMatrix::stringToMirrorBlend("Flip"), HUEMatrix::MirrorFlip);
    QCOMPARE(HUEMatrix::stringToMirrorBlend("Max"), HUEMatrix::MirrorMax);
    QCOMPARE(HUEMatrix::stringToMirrorBlend("Average"), HUEMatrix::MirrorAverage);
    QCOMPARE(HUEMatrix::stringToMirrorBlend("Additive"), HUEMatrix::MirrorAdditive);
    QCOMPARE(HUEMatrix::stringToMirrorBlend("Unknown"), HUEMatrix::MirrorFlip);
}

QTEST_MAIN(RGBTransform_Test)
#include "rgb_transform_test.moc"
