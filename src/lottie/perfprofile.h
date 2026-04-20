#ifndef PERFPROFILE_H
#define PERFPROFILE_H

#include <chrono>
#include <cstdint>

namespace rlottie {
namespace internal {

enum class ProfileEvent : uint8_t {
    CompositionUpdate,
    CompositionRender,
    CompLayerUpdateContent,
    ShapeLayerUpdateContent,
    PaintUpdateRenderNode,
    RenderMatteLayer,
    TrimPathUpdate,
    BitmapEffectApply,
    DashApply,
    RasterFill,
    RasterStroke,
    RasterStrokeSetup,
    RasterRender,
    DrawRleSolid,
    DrawRleGradient,
    DrawRleTexture,
    Count
};

bool performanceStatsEnabled();
void recordProfileEvent(ProfileEvent event, uint64_t elapsedNs);

struct ScopedProfileEvent {
    explicit ScopedProfileEvent(ProfileEvent event)
        : mEvent(event), mEnabled(performanceStatsEnabled())
    {
        if (mEnabled) mStart = std::chrono::steady_clock::now();
    }

    ~ScopedProfileEvent()
    {
        if (!mEnabled) return;

        auto elapsed = std::chrono::steady_clock::now() - mStart;
        auto elapsedNs = uint64_t(
            std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed)
                .count());
        recordProfileEvent(mEvent, elapsedNs);
    }

private:
    ProfileEvent                          mEvent;
    bool                                  mEnabled{false};
    std::chrono::steady_clock::time_point mStart{};
};

}  // namespace internal
}  // namespace rlottie

#endif  // PERFPROFILE_H
