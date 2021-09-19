import CoreMedia
import Foundation

public class FramesExtractor {
    public enum Error: Swift.Error {
        case noMoreFramesAvailable
        case unableToRenderAnimationFrame
    }

    let animation: Animation
    let renderer: Renderer

    let config: FramesExtractorConfig

    private var nextFrameIndex = 0
    private var nextFramePresentationTime: CMTime = .zero

    public init?(animation: Animation, config: FramesExtractorConfig) {
        guard let renderer = Renderer(
                width: config.outputWidth,
                height: config.outputHeight,
                scale: config.scale
        ) else {
            return nil
        }

        self.animation = animation
        self.renderer = renderer
        self.config = config
    }

    public func finished() -> Bool {
        return !config.allowsLooping && nextFrameIndex >= animation.numberOfFrames
    }

    public func reset() {
        nextFrameIndex = 0
        nextFramePresentationTime = .zero
    }

    public func nextFrame() throws -> AnimationFrame {
        if nextFrameIndex >= animation.numberOfFrames {
            guard config.allowsLooping else {
                throw Error.noMoreFramesAvailable
            }

            nextFrameIndex = 0
        }

        guard let image = renderer.render(
                animation: animation,
                frameIndex: nextFrameIndex,
                visibleRect: config.visibleRect
        ) else {
            throw Error.unableToRenderAnimationFrame
        }

        let frame = AnimationFrame(
            image: image,
            presentationTime: nextFramePresentationTime,
            duration: animation.framesDuration
        )

        nextFrameIndex += 1
        nextFramePresentationTime = nextFramePresentationTime + animation.framesDuration

        return frame
    }
}
