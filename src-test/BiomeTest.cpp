// Tests for the biome and surface builder registries: a biome that exists only in C++ (no behaviour
// pack), placed by taking over a vanilla biome's climate cells, with a surface builder whose shape is
// code. Both entries are deferred, exactly as a mod would declare them.

#include "Global.h"
#include "TestReport.hpp"
#include <ll/api/service/Bedrock.h>
#include <mc/world/level/Level.h>
#include <mc/world/level/biome/Biome.h>
#include <mc/world/level/biome/components/BiomeComponentStorage.h>
#include <mc/world/level/biome/GetBiomeOptions.h>
#include <mc/world/level/biome/source/BiomeArea.h>
#include <mc/world/level/biome/source/BiomeSource.h>
#include <mc/world/level/dimension/Dimension.h>
#include <mc/world/level/dimension/DimensionType.h>
#include <mc/world/level/levelgen/WorldGenerator.h>
#include <mc/world/level/levelgen/structure/BoundingBox.h>
#include <array>
#include <atomic>
#include <ll/api/event/world/ServerLevelTickEvent.h>
#include <ll/api/io/Logger.h>
#include <mc/world/level/BlockPos.h>
#include <mc/world/level/BlockSource.h>
#include <mc/world/level/ChunkPos.h>
#include <mc/world/level/block/Block.h>
#include <mc/world/level/block/BlockVolume.h>
#include <mc/world/level/block/registry/BlockTypeRegistry.h>
#include <mc/world/level/chunk/ChunkSource.h>
#include <mc/world/level/chunk/HeightmapWrapper.h>
#include "modapi/DeferredRegister.h"
#include "modapi/worldgen/BiomeRegistry.h"
#include "modapi/worldgen/SurfaceBuilderRegistry.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>

#include "modapi/addons/RuntimePack.h"

namespace {

// The columns the builder marked, plus the result of reading one of them back. The surface volume is
// a staging buffer, so a write only counts if the block is still there once the chunk is in the level.
//
// Several columns are kept because the engine also builds chunks it throws away again - the neighbours
// it pulls in as context while generating the chunk that was actually asked for. Those writes never
// reach a stored chunk, so the test walks the markers until it finds one whose chunk the level holds.
struct SurfaceMarkerEntry {
    int mX      = 0;
    int mY      = 0;
    int mZ      = 0;
    int mBottom = 0;
};

struct SurfaceMarker {
    static constexpr size_t kCapacity = 64;

    inline static std::array<SurfaceMarkerEntry, kCapacity> mEntries{};
    inline static std::atomic<size_t>                       mCount{0};
    inline static std::atomic_flag                          mLock = ATOMIC_FLAG_INIT;
    inline static std::atomic<int>                          mTries{0};
    inline static std::atomic<int>  mReadBack{-1}; // -1 not read yet, 0 gone, 1 the marker is there
    inline static std::atomic<size_t> mDecided{0};

    // One column per chunk: the columns of a chunk all share its fate, so a second one from the same
    // chunk would only crowd the list.
    static void remember(int x, int y, int z, int bottom) {
        auto const chunkX = x >> 4;
        auto const chunkZ = z >> 4;

        while (mLock.test_and_set(std::memory_order_acquire)) {}
        auto const count = mCount.load(std::memory_order_relaxed);
        bool       seen  = false;
        for (size_t i = 0; i < count; ++i) {
            if ((mEntries[i].mX >> 4) == chunkX && (mEntries[i].mZ >> 4) == chunkZ) {
                seen = true;
                break;
            }
        }
        if (!seen && count < kCapacity) {
            mEntries[count] = SurfaceMarkerEntry{x, y, z, bottom};
            mCount.store(count + 1, std::memory_order_relaxed);
        }
        mLock.clear(std::memory_order_release);
    }
};

// Reports that the engine reached it; a real builder would write through `column.mBlocks`.
class StoneSurfaceBuilder final : public modapi::worldgen::ICustomSurfaceBuilder {
public:
    void buildSurfaceAt(
        ::ISurfaceBuilder::GlobalParameters const&,
        ::ISurfaceBuilder::SurfaceBuilderParameters const& surfaceBuilderParams,
        ::ISurfaceBuilder::PerColumnParameters const&      columnParams
    ) const override {
        auto const n = mCalls.fetch_add(1, std::memory_order_relaxed);

        // The volume is one chunk column: 16 x 320 x 16, indexed y-first - the engine writes with
        // `index = y + height * (z + x * depth)` on volume local coordinates (see
        // `BlockVolumeTarget::setBlock`), and `mDimensionBottom` is the world y of local y 0.
        auto& volume = columnParams.mBlocks;
        auto  width  = static_cast<unsigned>(volume.mWidth);
        auto  height = static_cast<unsigned>(volume.mHeight);
        auto  depth  = static_cast<unsigned>(volume.mDepth);
        auto  bottom = static_cast<int>(volume.mDimensionBottom);
        if (width != 16 || depth != 16 || height == 0) return;

        auto const& pos     = columnParams.mPos;
        auto const& heights = *columnParams.mPreWorldGenHeightmap.mData;
        auto        column  = static_cast<size_t>((pos.z & 15) * 16 + (pos.x & 15));
        if (column >= heights.size()) return;

        // The marker goes on top of the column, which is where the surface would be.
        auto const top   = static_cast<int>(heights[column]);
        auto const localY = top - bottom;
        if (localY < 0 || localY >= static_cast<int>(height)) return;

        auto const localX = static_cast<unsigned>(pos.x & 15);
        auto const localZ = static_cast<unsigned>(pos.z & 15);
        auto const slot   = static_cast<size_t>(localY)
                        + static_cast<size_t>(height) * (localZ + static_cast<size_t>(localX) * depth);

        auto&      span     = volume.mBlocks.get();
        auto const capacity = static_cast<size_t>(span.mEnd - span.mBegin);
        if (slot >= capacity) return;

        auto const* marker = ::BlockTypeRegistry::get().lookupByName(::HashedString{"minecraft:gold_block"}, {}, false);
        if (marker == nullptr) return;
        span.mBegin[slot] = marker;
        SurfaceMarker::remember(pos.x, top, pos.z, bottom);

        if (n < 4) {
            test::getLogger().info(
                "[BIOME] surface #{} pos=({},{},{}) top={} local=({},{},{}) slot={}/{} wrote gold at ({},{},{})",
                n,
                pos.x,
                pos.y,
                pos.z,
                top,
                localX,
                localY,
                localZ,
                slot,
                capacity,
                pos.x,
                top,
                pos.z
            );
        }
    }

    inline static std::atomic<uint64_t> mCalls{0};
};

// A component ModAPI owns: the engine never looks its id up, so it is the safe way to exercise the
// component injection without guessing an engine key.
struct MarkerComponent : ::BiomeComponentBase {
    int mValue = 42;
};

constexpr unsigned short kMarkerKey = 60000;

test::Report report("BIOME", "biome.txt");

::Dimension* overworldDimension() {
    auto level = ll::service::getLevel();
    if (!level) return nullptr;

    auto dimension = level->getDimension(::DimensionType{0}).lock();
    if (!dimension) return nullptr;
    return dimension.get();
}

// Reads one of the remembered markers back from the level. Returns -1 while none of them can be read
// (nothing built yet, the level is not there, or no marker's chunk is resident), else 1 when the
// marker survived the commit into the chunk and 0 when it did not.
int probeMarkerReadBack() {
    auto const known = SurfaceMarker::mReadBack.load(std::memory_order_relaxed);
    if (known >= 0) return known;
    if (SurfaceMarker::mCount.load(std::memory_order_relaxed) == 0) return -1;

    auto* dimension = overworldDimension();
    if (dimension == nullptr) return -1;
    auto& source = dimension->getBlockSourceFromMainChunkSource();
    if (source.getDimensionId() != 0) return -1;

    auto& air  = ::BlockTypeRegistry::get().getDefaultBlockState(::HashedString{"minecraft:air"}, false);
    auto& gold = ::BlockTypeRegistry::get().getDefaultBlockState(::HashedString{"minecraft:gold_block"}, false);

    for (size_t i = 0; i < SurfaceMarker::mCount.load(std::memory_order_relaxed); ++i) {
        auto const& entry = SurfaceMarker::mEntries[i];

        // Only a chunk the level still holds can answer for the write: the neighbours the engine pulls
        // in for context are discarded again, and reading those answers air.
        auto chunk = dimension->getChunkSource().getExistingChunk(::ChunkPos{entry.mX >> 4, entry.mZ >> 4});
        if (!chunk) continue;

        ::BlockPos const pos{entry.mX, entry.mY, entry.mZ};

        // A chunk that is still being built reads as air all the way down, so the floor is the
        // readiness check: until it is there, nothing about this column can be decided.
        auto const& floor = source.getBlock(::BlockPos{pos.x, entry.mBottom, pos.z});
        if (&floor == &air) continue;

        auto const& block = source.getBlock(pos);
        bool const  hit   = &block == &gold;

        SurfaceMarker::mDecided.store(i, std::memory_order_relaxed);
        SurfaceMarker::mReadBack.store(hit ? 1 : 0, std::memory_order_relaxed);
        test::getLogger().info(
            "[BIOME] readback #{}/{} at ({},{},{}) -> {} ({}) [{} {} {} {}]",
            i,
            SurfaceMarker::mCount.load(std::memory_order_relaxed),
            pos.x,
            pos.y,
            pos.z,
            block.getTypeName(),
            hit ? "HIT" : "MISS",
            source.getBlock(::BlockPos{pos.x, pos.y - 1, pos.z}).getTypeName(),
            source.getBlock(::BlockPos{pos.x, pos.y - 2, pos.z}).getTypeName(),
            source.getBlock(::BlockPos{pos.x, pos.y - 3, pos.z}).getTypeName(),
            floor.getTypeName()
        );
        return hit ? 1 : 0;
    }
    return -1;
}

// A write into the surface volume is only worth anything once the chunk holds it, so the markers are
// read back from the level a few ticks after the columns were built: the delay leaves room for the
// engine to commit the volume into the chunk.
void installSurfaceReadBack() {
    test::listen<ll::event::world::ServerLevelTickEvent>([](ll::event::world::ServerLevelTickEvent&) {
        if (SurfaceMarker::mReadBack.load(std::memory_order_relaxed) >= 0) return;
        if (SurfaceMarker::mCount.load(std::memory_order_relaxed) == 0) return;
        if (SurfaceMarker::mTries.fetch_add(1, std::memory_order_relaxed) < 20) return;
        probeMarkerReadBack();
    });
}

std::filesystem::path gClientPackRoot;
std::string           gClientPackUuid;
bool                  gClientPackInstalled = false;

std::string uuidInManifest(std::filesystem::path const& manifestPath) {
    std::ifstream     in(manifestPath, std::ios::binary);
    std::stringstream buffer;
    buffer << in.rdbuf();

    auto const text = buffer.str();
    auto const key  = text.find("\"uuid\"");
    if (key == std::string::npos) return {};

    auto const first = text.find('"', text.find(':', key) + 1);
    auto const last  = text.find('"', first + 1);
    if (first == std::string::npos || last == std::string::npos) return {};
    return text.substr(first + 1, last - first - 1);
}

std::string fogFile(std::string const& identifier, std::string const& air, std::string const& water) {
    return R"({"format_version":"1.16.100","minecraft:fog_settings":{)"
           R"("description":{"identifier":")"
         + identifier + R"("},)"
                        R"("distance":{"air":{"fog_start":0.2,"fog_end":0.85,"fog_color":")"
         + air + R"(","render_distance_type":"render"},)"
                 R"("water":{"fog_start":0.1,"fog_end":0.9,"fog_color":")"
         + water + R"(","render_distance_type":"render"}}}})";
}

std::string biomesClientFile() {
    return R"({"biomes":{)"
           R"("mymod:cpp_biome":{"water_surface_color":"#3F76E4","water_fog_color":"#050533",)"
           R"("fog_identifier":"mymod:cpp_biome_fog","inherit_from_prior_fog":false},)"
           R"("mymod:cpp_biome2":{"water_surface_color":"#4E9AD1","water_fog_color":"#071A2B",)"
           R"("fog_identifier":"mymod:cpp_biome2_fog","inherit_from_prior_fog":false},)"
           R"("mymod:climate_biome":{"water_surface_color":"#2E8B57","water_fog_color":"#08290F",)"
           R"("fog_identifier":"mymod:climate_biome_fog","inherit_from_prior_fog":false}}})";
}

void installClientPack() {
    modapi::addons::RuntimePack pack{"modapi_biome_test_client", "resources"};
    pack.addFile("biomes_client.json", biomesClientFile());
    // One fog per biome, tinted so that walking into a biome is visible on its own: the air fog colour
    // is what a player sees without needing water or a specific time of day.
    pack.addFile("fogs/mymod_cpp_biome_fog.json", fogFile("mymod:cpp_biome_fog", "#5A1010", "#0B2E33"));
    pack.addFile("fogs/mymod_cpp_biome2_fog.json", fogFile("mymod:cpp_biome2_fog", "#101E5A", "#0E2740"));
    pack.addFile("fogs/mymod_climate_biome_fog.json", fogFile("mymod:climate_biome_fog", "#0F5A28", "#0A3312"));
    gClientPackInstalled = pack.install();
    test::getLogger().info("[BIOME] client pack installed: {}", gClientPackInstalled);

    gClientPackRoot = pack.root();
    gClientPackUuid = uuidInManifest(gClientPackRoot / "manifest.json");
    test::getLogger().info("[BIOME] client pack {} at {}", gClientPackUuid, gClientPackRoot.string());
}


} // namespace

// Both have to be alive before their registry publishes its ready event, which happens while the
// level configures its biomes - so they are static, as they would be in a mod.
static modapi::DeferredRegister<modapi::worldgen::SurfaceBuilderRegistry, StoneSurfaceBuilder> gStoneSurface{
    modapi::worldgen::SurfaceBuilderRegistration<>{
        .mIdentifier = "mymod:stone",
        .mArguments  = {},
    }
};

static modapi::DeferredRegister<modapi::worldgen::BiomeRegistry, modapi::worldgen::BiomeRegistration> gCppBiome{
    modapi::worldgen::BiomeRegistration{
        .mIdentifier = "mymod:cpp_biome",
        .mProperty =
            {
                .mTemplate = "minecraft:plains",
                .mSurface = std::string{"mymod:stone"},
            },
    }
};

// A biome that takes over a small vanilla biome instead of claiming a cell of its own: a full-range
// cell out-specifies every vanilla cell and would shadow the whole world (see the README), which
// would leave nothing else to locate.
static modapi::DeferredRegister<modapi::worldgen::BiomeRegistry, modapi::worldgen::BiomeRegistration> gClimateBiome{
    modapi::worldgen::BiomeRegistration{
        .mIdentifier = "mymod:climate_biome",
        .mProperty =
            {
                .mTemplate = "minecraft:desert",
                .mClimate = modapi::worldgen::BiomeProperty::Replacement{
                    .mTargetBiomes = {"minecraft:savanna"},
                },
                .mComponents =
                    {
                        {
                            .mKey    = ::Bedrock::typeid_t<void>{kMarkerKey},
                            .mCreate = [] { return ::std::make_unique<MarkerComponent>(); },
                        },
                    },
                .mSurface = std::string{"mymod:stone"},
            },
    }
};

namespace {

int runBiomeTests() {
    report.reset();
    test::getLogger().info("[BIOME] ===== biome suite start =====");
    report.write();

    report.section("deferred", [&] {
        report.isTrue("surfaceBuilderRegistered", gStoneSurface.isRegistered());
        report.isTrue("biomeRegistered", gCppBiome.isRegistered());
        report.isTrue("climateBiomeRegistered", gClimateBiome.isRegistered());
        report.isTrue("surfaceBuilderProduct", gStoneSurface.get().has_value());
        report.isTrue("biomeProduct", gCppBiome.get().has_value());
    });

    report.section("registry", [&] {
        auto& biomes = modapi::worldgen::BiomeRegistry::getInstance();
        report.isTrue("biomeRegistryReady", biomes.isReady());
        report.isTrue("biomeLookup", biomes.getBiome("mymod:cpp_biome").has_value());
        report.isTrue("climateBiomeLookup", biomes.getBiome("mymod:climate_biome").has_value());

        auto climate = gClimateBiome.get();
        size_t markerCount = 0;
        if (climate) {
            for (auto& [key, component] : climate->mBiomeComponentStorage.get().mComponentList.get()) {
                if (key.value == kMarkerKey) ++markerCount;
            }
        }
        report.isTrue("component.installed", markerCount == 1, fmt::format("{} entries", markerCount));

        // What the engine filed each biome's components under; the ids are per process and not
        // exported, so this dump is how a mod learns the key of a component it wants to add.
        modapi::worldgen::BiomeRegistry::dumpComponentKeys();
    });

    report.section("climate", [&] {
        // Ask the biome source itself: it is what every consumer (chunk generation, /locate) reads.
        auto level = ::ll::service::getLevel();
        if (!level) {
            report.skip("climate.noLevel");
            return;
        }
        auto dimension = level->getDimension(::DimensionType{0 }).lock();
        if (!dimension || dimension->mWorldGenerator == nullptr) {
            report.skip("climate.noGenerator");
            return;
        }
        auto const& source = dimension->mWorldGenerator->getBiomeSource();

        ::std::unordered_map<::Biome const*, size_t> counts;
        size_t                                       samples = 0;
        // The biome source of a multinoise world is pure noise: asking it needs no chunk, unlike the
        // `getBiomeArea` path that reads loaded data.
        for (int x = -1024; x <= 1024; x += 32) {
            for (int z = -1024; z <= 1024; z += 32) {
                auto const* biome = source._getBiome(::GetBiomeOptions{::BlockPos{x, 64, z}, false});
                ++samples;
                if (biome != nullptr) ++counts[biome];
            }
        }
        auto count = [&](::modapi::worldgen::BiomeRegistry::ProductRef product) -> size_t {
            auto* biome = product.as_ptr();
            auto  it    = biome != nullptr ? counts.find(biome) : counts.end();
            return it == counts.end() ? 0 : it->second;
        };

        auto cpp = count(gCppBiome.get());
        auto own = count(gClimateBiome.get());
        test::getLogger().info(
            "[BIOME] biome source over {} samples: cpp_biome={} climate_biome={}",
            samples,
            cpp,
            own
        );
        // The grid is 64x64 chunks, which is smaller than some biomes: the numbers are reported so a
        // miss is visible, and only the presence of at least one of our biomes is asserted.
        report.isTrue("climate.places", cpp + own > 0, fmt::format("{} + {}", cpp, own));
        report.isTrue("climate.sourceAnswers", cpp + own > 0, fmt::format("{} + {}", cpp, own));
    });

    report.section("clientPack", [&] {
        // Installed while the mod loaded, which is before the server composes the pack stack it offers
        // to clients.
        report.isTrue("pack.installed", gClientPackInstalled);
        report.isTrue("pack.manifest", std::filesystem::exists(gClientPackRoot / "manifest.json"));
        report.isTrue("pack.uuid", !gClientPackUuid.empty());

        auto const biomes = gClientPackRoot / "biomes_client.json";
        report.isTrue("pack.biomesClient", std::filesystem::exists(biomes));

        std::ifstream     in(biomes, std::ios::binary);
        std::stringstream buffer;
        buffer << in.rdbuf();
        auto const text = buffer.str();
        report.isTrue("pack.cppBiome", text.find("mymod:cpp_biome\"") != std::string::npos);
        report.isTrue("pack.cppBiome2", text.find("mymod:cpp_biome2\"") != std::string::npos);
        report.isTrue("pack.climateBiome", text.find("mymod:climate_biome\"") != std::string::npos);

        report.isTrue(
            "pack.fog",
            std::filesystem::exists(gClientPackRoot / "fogs" / "mymod_cpp_biome_fog.json")
        );
    });

    report.section("engine", [&] {
        // The builder only runs for chunks generated after it was installed. The test world already
        // exists, so a run without a client growing new chunks has nothing to show - a skip, not a
        // failure.
        auto const calls = StoneSurfaceBuilder::mCalls.load();
        if (calls == 0) report.skip("surfaceBuilder.called");
        else report.isTrue("surfaceBuilder.called", true, fmt::format("{} columns", calls));

        // The write itself proves nothing; the block has to be readable from the level afterwards.
        auto const readBack = probeMarkerReadBack();
        if (readBack < 0) {
            report.skip("surfaceBuilder.readBack");
        } else {
            auto const& entry = SurfaceMarker::mEntries[SurfaceMarker::mDecided.load()];
            report.isTrue(
                "surfaceBuilder.readBack",
                readBack == 1,
                fmt::format(
                    "({},{},{}) {}",
                    entry.mX,
                    entry.mY,
                    entry.mZ,
                    readBack == 1 ? "holds the marker" : "lost the marker"
                )
            );
        }
    });

    report.finish();
    return report.failed();
}

void registerBiomeTestCommand() { registerTestFunction("biome", [] { return runBiomeTests() == 0; }); }

} // namespace

// A second takeover biome, registered after the own-cell one: it tells whether the command's biome
// enum only takes the biomes of the first registration pass.
static modapi::DeferredRegister<modapi::worldgen::BiomeRegistry, modapi::worldgen::BiomeRegistration> gCppBiome2{
    modapi::worldgen::BiomeRegistration{
        .mIdentifier = "mymod:cpp_biome2",
        .mProperty =
            {
                .mTemplate = "minecraft:forest",
                .mSurface  = std::string{"mymod:stone"},
            },
    }
};

REGISTER_ON_LOAD_TEST(BiomeClientPack, installClientPack)
REGISTER_ON_LOAD_TEST(BiomeSurfaceReadBack, installSurfaceReadBack)
REGISTER_ON_LOAD_TEST(BiomeCommand, registerBiomeTestCommand)
REGISTER_TEST(Biome, runBiomeTests)
