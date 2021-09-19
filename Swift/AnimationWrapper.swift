import CoreMedia
import Foundation

import rlottie

class AnimationWrapper {
    let animation: OpaquePointer

    let numberOfFrames: Int
    let framesDuration: CMTime
    let totalDuration: CMTime
    let width: Int
    let height: Int

    private init?(animation: OpaquePointer) {
        self.animation = animation

        numberOfFrames = lottie_animation_get_totalframe(animation)

        guard numberOfFrames > 0 else {
            return nil
        }

        let totalDuration = lottie_animation_get_duration(animation)
        self.totalDuration = CMTime(seconds: totalDuration, preferredTimescale: 600)
        framesDuration = CMTime(seconds: totalDuration / Double(numberOfFrames), preferredTimescale: 600)

        var width = 0
        var height = 0
        lottie_animation_get_size(animation, &width, &height)
        self.width = width
        self.height = height
    }

    convenience init?(data: Data) {
        lottie_configure_model_cache_size(0)

        guard let animation = data.withUnsafeBytes({ bufferPointer in
            lottie_animation_from_data(bufferPointer.bindMemory(to: CChar.self).baseAddress, "", "")
        }) else {
            return nil
        }

        self.init(animation: animation)
    }

    convenience init?(string: String) {
        lottie_configure_model_cache_size(0)

        guard let cString = string.cString(using: .utf8),
              let animation = lottie_animation_from_data(cString, "", "") else {
            return nil
        }

        self.init(animation: animation)
    }

    func render(
        frameIndex: Int,
        buffer: UnsafeMutablePointer<UInt32>,
        width: Int,
        height: Int,
        bytesPerRow: Int
    ) {
        lottie_animation_render(animation, frameIndex, buffer, width, height, bytesPerRow)
    }

    deinit {
        lottie_animation_destroy(animation)
    }
}
