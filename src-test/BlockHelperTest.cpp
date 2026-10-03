// Tests for BlockHelper - the region handle a world generation feature is handed.
//
// The cases run on a live server. Handle validity, the region identity, the height range and the
// copy/move semantics only need the overworld's BlockSource, so they always run; the cases that
// actually read and write blocks need a *loaded* chunk, which a server without players may not have
// yet - those are reported as skipped instead of failing (run `modapitest blockhelper` in game to
// cover them). A write case puts a block into the world and restores the previous block right away,
// and it picks a position well above the terrain so nothing else notices.

#include "Global.h"
#include "TestReport.hpp"
#include <mc/server/SimulatedPlayer.h>
#include <mc/deps/core/math/Vec2.h>
#include <mc/deps/core/math/Vec3.h>
#include <mc/world/level/dimension/DimensionType.h>
#include "modapi/worldgen/BlockHelper.h"
#include <algorithm>
#include <cstdlib>
#include <ll/api/service/Bedrock.h>
#include <mc/world/level/BlockSource.h>
#include <mc/world/level/Level.h>
#include <mc/world/level/block/Block.h>
#include <mc/world/level/block/registry/BlockTypeRegistry.h>
#include <mc/world/level/chunk/LevelChunk.h>
#include <mc/world/level/dimension/Dimension.h>
#include <optional>
#include <string>

namespace {

using modapi::BlockHelper;

test::Report report{"[BLOCKHELPER]", "blockhelper-test-report.txt"};

// A position inside a loaded chunk, plus the chunk it belongs to.
struct TestSpot {
    ::LevelChunk* mChunk = nullptr;
    BlockPos      mPos{};
};

::BlockSource* overworldBlockSource() {
    auto level = ll::service::getLevel();
    if (!level) return nullptr;

    // The overworld; the lock also covers the (unlikely) case of the dimension being gone.
    auto dimension = level->getDimension(::DimensionType{0}).lock();
    if (!dimension) return nullptr;

    auto& source = dimension->getBlockSourceFromMainChunkSource();
    // Guard against a build whose dimension ids are not the well known ones.
    if (source.getDimensionId() != 0) return nullptr;
    return &source;
}

// A virtual player keeps the chunks around its position loaded, which is what the write cases need; it
// takes no client, and removes itself again (also when an assertion throws).
class TestAnchor {
    ::SimulatedPlayer* mPlayer = nullptr;

public:
    TestAnchor() {
        auto level = ll::service::getLevel();
        if (!level) return;

        auto const spawn   = level->getSharedSpawnPos();
        auto       created = ::SimulatedPlayer::create(
            "ModApiTestBlock",
            ::Vec3{
                static_cast<float>(spawn.x) + 0.5f,
                static_cast<float>(spawn.y) + 2.0f,
                static_cast<float>(spawn.z) + 0.5f
            },
            ::DimensionType{0},
            ::Vec2{0.0f, 0.0f}
        );
        if (created) mPlayer = created.as_ptr();
    }

    ~TestAnchor() {
        if (mPlayer != nullptr) mPlayer->remove();
    }

    TestAnchor(TestAnchor const&)            = delete;
    TestAnchor& operator=(TestAnchor const&) = delete;

    [[nodiscard]] bool valid() const { return mPlayer != nullptr; }
};
::Block const* blockByName(char const* name) { return ::BlockTypeRegistry::get().lookupByName(::HashedString{name}, {}, false); }

// Nearest loaded chunk to the spawn point, and a position inside it 40 blocks above the spawn
// height. Returns nothing when the server has no loaded chunk there (no player has been near it).
std::optional<TestSpot> findTestSpot(::BlockSource& source) {
    auto level = ll::service::getLevel();
    if (!level) return std::nullopt;

    auto const spawn  = level->getSharedSpawnPos();
    auto const chunkX = spawn.x >> 4;
    auto const chunkZ = spawn.z >> 4;
    auto const minY   = static_cast<short>(source.mDimension.mHeightRange->mMin);
    auto const maxY   = static_cast<short>(source.mDimension.mHeightRange->mMax);
    auto const y      = static_cast<short>(std::clamp<int>(spawn.y + 40, minY + 1, maxY - 2));

    for (int radius = 0; radius <= 4; ++radius) {
        for (int dx = -radius; dx <= radius; ++dx) {
            for (int dz = -radius; dz <= radius; ++dz) {
                if (std::max(std::abs(dx), std::abs(dz)) != radius) continue; // only the ring

                auto* chunk = source.getChunk(chunkX + dx, chunkZ + dz);
                if (chunk == nullptr) continue;

                auto const chunkPos = *chunk->mPosition;
                BlockPos   pos{chunkPos.x * 16 + 8, y, chunkPos.z * 16 + 8};
                if (!source.hasChunksAt(pos, 0, false)) continue;
                return TestSpot{chunk, pos};
            }
        }
    }
    return std::nullopt;
}

// Writes `replacement` at `pos` through `helper`, checks that it is readable, and restores the
// previous block.
void checkWriteRoundTrip(std::string const& prefix, BlockHelper& helper, BlockPos const& pos, ::Block const& replacement) {
    auto const original = helper.getBlock(pos);
    report.isTrue(prefix + ".readOriginal", original.has_value());
    if (!original) return;

    report.isTrue(prefix + ".write", helper.setBlock(pos, replacement));
    report.isTrue(prefix + ".readBack", helper.getBlock(pos).as_ptr() == &replacement);
    report.isTrue(prefix + ".restore", helper.setBlock(pos, original.value()));
    report.isTrue(prefix + ".readRestored", helper.getBlock(pos).as_ptr() == original.as_ptr());
}

void testEmptyHandle() {
    report.section("empty", [] {
        BlockHelper empty;

        report.isTrue("isValid", !empty.isValid());
        report.isTrue("holdsBlockSource", !empty.holdsBlockSource());
        report.isTrue("holdsLevelChunk", !empty.holdsLevelChunk());
        report.isTrue("getBlockSource", empty.getBlockSource() == nullptr);
        report.isTrue("getLevelChunk", empty.getLevelChunk() == nullptr);
        report.isTrue("isValidPosition", !empty.isValidPosition({0, 0, 0}));
        report.isTrue("getBlock", !empty.getBlock({0, 0, 0}).has_value());

        // An empty handle has no height range, and asking for one must not crash.
        auto const range = empty.getHeightRange();
        report.isTrue("heightRange", empty.getMinHeight() == range.mMin && empty.getMaxHeight() == range.mMax);

        // An empty handle rejects a write without throwing; the ERR line it logs is expected here.
        test::getLogger().info("[BLOCKHELPER] (the next ERR line is expected: writing through an empty handle)");
        report.noThrow("noThrow", [&empty] {
            auto const& stone = *blockByName("minecraft:stone");
            report.isTrue("setBlock", !empty.setBlock({0, 64, 0}, stone));
        });
    });
}

void testNullHandle() {
    report.section("nullRegion", [] {
        // A handle built from a null region is empty, and every operation has to fail instead of
        // dereferencing it (this is the crash the refactor removed).
        BlockHelper nullSource{static_cast<::BlockSource*>(nullptr)};
        BlockHelper nullChunk{static_cast<::LevelChunk*>(nullptr)};

        report.isTrue("source.isValid", !nullSource.isValid());
        report.isTrue("source.getBlockSource", nullSource.getBlockSource() == nullptr);
        report.isTrue("source.isValidPosition", !nullSource.isValidPosition({0, 64, 0}));
        report.isTrue("source.getBlock", !nullSource.getBlock({0, 64, 0}).has_value());
        report.isTrue("chunk.isValid", !nullChunk.isValid());
        report.isTrue("chunk.getLevelChunk", nullChunk.getLevelChunk() == nullptr);
        report.isTrue("chunk.getBlock", !nullChunk.getBlock({0, 64, 0}).has_value());

        report.noThrow("noThrow", [&] {
            auto const& stone = *blockByName("minecraft:stone");
            report.isTrue("source.setBlock", !nullSource.setBlock({0, 64, 0}, stone));
            report.isTrue("chunk.setBlock", !nullChunk.setBlock({0, 64, 0}, stone));
        });
    });
}

void testHeightRange() {
    report.section("heightRange", [] {
        auto* source = overworldBlockSource();
        if (source == nullptr) {
            report.skip("source");
            return;
        }

        BlockHelper helper{source};
        auto const  minY = static_cast<short>(source->mDimension.mHeightRange->mMin);
        auto const  maxY = static_cast<short>(source->mDimension.mHeightRange->mMax);
        report.isTrue("minMatches", helper.getMinHeight() == minY, fmt::format("{} vs {}", helper.getMinHeight(), minY));
        report.isTrue("maxMatches", helper.getMaxHeight() == maxY, fmt::format("{} vs {}", helper.getMaxHeight(), maxY));
        report.isTrue("rangeMatches", helper.getHeightRange().mMin == minY && helper.getHeightRange().mMax == maxY);

        // The height check alone does not need a loaded chunk, so these run anywhere: the range is
        // half open - `mMin` is inside, `mMax` is not.
        report.isTrue("atMinInside", !helper.isValidPosition({0, static_cast<int>(minY) - 1, 0}));
        report.isTrue("atMaxOutside", !helper.isValidPosition({0, static_cast<int>(maxY), 0}));
        report.isTrue("farAboveOutside", !helper.isValidPosition({0, 32000, 0}));
        report.isTrue("farBelowOutside", !helper.isValidPosition({0, -32000, 0}));
    });
}

void testBlockSourceHandle() {
    report.section("blockSource", [] {
        auto* source = overworldBlockSource();
        if (source == nullptr) {
            test::getLogger().warn("[BLOCKHELPER] no overworld BlockSource; blockSource cases skipped");
            report.skip("blockSource.noSource");
            return;
        }

        BlockHelper helper{source};
        report.isTrue("isValid", helper.isValid());
        report.isTrue("holdsBlockSource", helper.holdsBlockSource());
        report.isTrue("notHoldsLevelChunk", !helper.holdsLevelChunk());
        report.isTrue("getBlockSource", helper.getBlockSource() == source);
        report.isTrue("getLevelChunk", helper.getLevelChunk() == nullptr);

        auto const range = helper.getHeightRange();
        report.isTrue("heightRangeMatches", helper.getMinHeight() == range.mMin && helper.getMaxHeight() == range.mMax);
        report.isTrue("heightRangeSane", helper.getMinHeight() < helper.getMaxHeight());

        // Outside the height range and outside the loaded region are both rejected.
        report.isTrue("aboveRange", !helper.isValidPosition({0, helper.getMaxHeight(), 0}));
        report.isTrue("belowRange", !helper.isValidPosition({0, helper.getMinHeight() - 1, 0}));
        report.isTrue("farPosition", !helper.isValidPosition({1000000, 64, 1000000}));
        report.isTrue("farRead", !helper.getBlock({1000000, 64, 1000000}).has_value());

        // A rejected write does not throw (it logs the ERR line announced here).
        test::getLogger().info("[BLOCKHELPER] (the next ERR line is expected: writing outside the region)");
        report.noThrow("noThrowOnFarWrite", [&] {
            auto const& stone = *blockByName("minecraft:stone");
            report.isTrue("farWrite", !helper.setBlock({1000000, 64, 1000000}, stone));
        });

        TestAnchor anchor;
        test::getLogger().info("[BLOCKHELPER] virtual player anchor: {}", anchor.valid());
        auto spot = findTestSpot(*source);
        if (!spot) {
            test::getLogger().warn("[BLOCKHELPER] no loaded chunk near the spawn point; write cases skipped");
            report.skip("write.noChunk");
            return;
        }
        report.isTrue("positionValid", helper.isValidPosition(spot->mPos));

        auto const* replacement = blockByName("minecraft:stone");
        if (replacement == nullptr) {
            test::getLogger().warn("[BLOCKHELPER] minecraft:stone is not in the block registry; write cases skipped");
            report.skip("write.noStone");
            return;
        }
        checkWriteRoundTrip("write", helper, spot->mPos, *replacement);

        // The extra block layer is readable and writable through the same handle. Writing the value
        // that is already there keeps the world untouched.
        report.noThrow("extraBlock", [&] {
            auto const extra = helper.getBlock(spot->mPos, BlockHelper::Layer::ExtraBlock);
            report.isTrue("extraBlock.read", extra.has_value());
            if (!extra) return;
            report.isTrue("extraBlock.write", helper.setBlock(spot->mPos, extra.value(), BlockHelper::Layer::ExtraBlock));
            report.isTrue(
                "extraBlock.readBack",
                helper.getBlock(spot->mPos, BlockHelper::Layer::ExtraBlock).as_ptr() == extra.as_ptr()
            );
        });

        // An out of range layer value is rejected without writing anything.
        auto const garbage = static_cast<BlockHelper::Layer>(7);
        auto const before  = helper.getBlock(spot->mPos);
        report.isTrue("garbageLayer.write", !helper.setBlock(spot->mPos, *replacement, garbage));
        report.isTrue("garbageLayer.read", !helper.getBlock(spot->mPos, garbage).has_value());
        report.isTrue("garbageLayer.unchanged", helper.getBlock(spot->mPos).as_ptr() == before.as_ptr());
    });
}

void testLevelChunkHandle() {
    report.section("levelChunk", [] {
        auto* source = overworldBlockSource();
        if (source == nullptr) {
            test::getLogger().warn("[BLOCKHELPER] no overworld BlockSource; levelChunk cases skipped");
            report.skip("levelChunk.noSource");
            return;
        }
        TestAnchor anchor;
        test::getLogger().info("[BLOCKHELPER] virtual player anchor: {}", anchor.valid());
        auto spot = findTestSpot(*source);
        if (!spot) {
            test::getLogger().warn("[BLOCKHELPER] no loaded chunk near the spawn point; levelChunk cases skipped");
            report.skip("levelChunk.noChunk");
            return;
        }

        BlockHelper helper{spot->mChunk};
        report.isTrue("isValid", helper.isValid());
        report.isTrue("holdsLevelChunk", helper.holdsLevelChunk());
        report.isTrue("notHoldsBlockSource", !helper.holdsBlockSource());
        report.isTrue("getLevelChunk", helper.getLevelChunk() == spot->mChunk);
        report.isTrue("positionValid", helper.isValidPosition(spot->mPos));

        // The regression this refactor fixed: a position *before* the chunk has a negative local
        // coordinate, which the old check accepted and then wrote to a block of the neighbour.
        auto const chunkPos = *spot->mChunk->mPosition;
        auto const before   = BlockPos{chunkPos.x * 16 - 1, spot->mPos.y, spot->mPos.z};
        auto const beyond   = BlockPos{chunkPos.x * 16 + 16, spot->mPos.y, spot->mPos.z};
        report.isTrue("rejectsBeforeChunk", !helper.isValidPosition(before));
        report.isTrue("rejectsBeyondChunk", !helper.isValidPosition(beyond));
        report.isTrue("rejectsAboveRange", !helper.isValidPosition({spot->mPos.x, helper.getMaxHeight(), spot->mPos.z}));
        report.isTrue("beforeChunkReadEmpty", !helper.getBlock(before).has_value());

        test::getLogger().info("[BLOCKHELPER] (the next ERR line is expected: writing before the chunk)");
        report.noThrow("noThrowOnBeforeChunkWrite", [&] {
            auto const& stone = *blockByName("minecraft:stone");
            report.isTrue("beforeChunkWrite", !helper.setBlock(before, stone));
        });

        // The neighbour really is a different region and *its* handle accepts that position.
        if (auto* neighbour = source->getChunkAt(before)) {
            report.isTrue("neighbourAccepts", BlockHelper{neighbour}.isValidPosition(before));
        } else {
            report.skip("neighbourChunk");
        }

        auto const* replacement = blockByName("minecraft:stone");
        if (replacement == nullptr) {
            test::getLogger().warn("[BLOCKHELPER] minecraft:stone is not in the block registry; write cases skipped");
            report.skip("write.noStone");
            return;
        }
        checkWriteRoundTrip("write", helper, spot->mPos, *replacement);

        // Both handle kinds wrap the same region: a write through the BlockSource handle is visible
        // through the chunk handle, and the other way round.
        BlockHelper sourceHelper{source};
        auto const  original = helper.getBlock(spot->mPos);
        if (original) {
            report.isTrue("crossHandle.sourceWrite", sourceHelper.setBlock(spot->mPos, *replacement));
            report.isTrue("crossHandle.chunkSees", helper.getBlock(spot->mPos).as_ptr() == replacement);
            report.isTrue("crossHandle.chunkWrite", helper.setBlock(spot->mPos, original.value()));
            report.isTrue("crossHandle.sourceSees", sourceHelper.getBlock(spot->mPos).as_ptr() == original.as_ptr());
        } else {
            report.skip("crossHandle");
        }

        report.noThrow("extraBlockRead", [&] { (void)helper.getBlock(spot->mPos, BlockHelper::Layer::ExtraBlock); });
    });
}

void testCopyAndMove() {
    report.section("copyAndMove", [] {
        auto* source = overworldBlockSource();
        if (source == nullptr) {
            test::getLogger().warn("[BLOCKHELPER] no overworld BlockSource; copy cases skipped");
            report.skip("copy.noSource");
            return;
        }

        BlockHelper original{source};

        // Copy construction and assignment refer to the same region.
        BlockHelper copy{original};
        report.isTrue("copyValid", copy.isValid());
        report.isTrue("copySameRegion", copy.getBlockSource() == original.getBlockSource());
        report.isTrue("copySameHeight", copy.getMinHeight() == original.getMinHeight());

        BlockHelper assigned;
        report.isTrue("assignedNotValidYet", !assigned.isValid());
        assigned = original;
        report.isTrue("assignedValid", assigned.isValid());
        report.isTrue("assignedSameRegion", assigned.getBlockSource() == original.getBlockSource());

        // Moving (and using the moved-from handle) must not crash: the handle owns nothing that a
        // move could take away, unlike the old one which held a `unique_ptr` implementation.
        auto const probe = BlockPos{0, 64, 0};
        BlockHelper moved{std::move(copy)};
        report.isTrue("movedValid", moved.isValid());
        report.isTrue("movedSameRegion", moved.getBlockSource() == original.getBlockSource());
        report.noThrow("movedFromUsable", [&] {
            (void)copy.isValid();
            (void)copy.getBlock(probe);
            (void)copy.isValidPosition(probe);
            (void)copy.getBlockSource();
        });

        BlockHelper moveAssigned;
        moveAssigned = std::move(moved);
        report.isTrue("moveAssignedValid", moveAssigned.isValid());
        report.noThrow("moveAssignedFromUsable", [&] { (void)moved.getBlock(probe); });

        // A write through a copy is visible through the original, because both wrap one region.
        auto        spot        = findTestSpot(*source);
        auto const* replacement = blockByName("minecraft:stone");
        if (!spot || replacement == nullptr) {
            test::getLogger().warn("[BLOCKHELPER] no loaded chunk or no minecraft:stone; write-through-copy case skipped");
            report.skip("copy.noChunkOrStone");
            return;
        }

        // The position is picked above the terrain, but make sure the block there really differs so
        // the write is observable.
        auto pos = spot->mPos;
        if (original.getBlock(pos).as_ptr() == replacement) pos.x += 1;

        auto const originalBlock = original.getBlock(pos);
        report.isTrue("readOriginal", originalBlock.has_value());
        if (!originalBlock) return;

        report.isTrue("copyWrite", copy.setBlock(pos, *replacement));
        report.isTrue("originalSeesWrite", original.getBlock(pos).as_ptr() == replacement);
        report.isTrue("copyRestore", copy.setBlock(pos, originalBlock.value()));
        report.isTrue("originalSeesRestore", original.getBlock(pos).as_ptr() == originalBlock.as_ptr());
        report.isTrue("assignedReads", assigned.getBlock(pos).has_value());
    });
}

int runBlockHelperTests() {
    report.reset();
    test::getLogger().info("[BLOCKHELPER] ===== block helper suite start =====");
    report.write();

    testEmptyHandle();
    testNullHandle();
    testHeightRange();
    testBlockSourceHandle();
    testLevelChunkHandle();
    testCopyAndMove();

    report.finish();
    return report.failed();
}

void registerBlockHelperTestCommand() { registerTestFunction("blockhelper", [] { return runBlockHelperTests() == 0; }); }

} // namespace

REGISTER_ON_LOAD_TEST(BlockHelperCommand, registerBlockHelperTestCommand)
REGISTER_TEST(BlockHelper, runBlockHelperTests)
