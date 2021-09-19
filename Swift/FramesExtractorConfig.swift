import CoreGraphics

public struct FramesExtractorConfig {
    public let allowsLooping: Bool
    public let visibleRect: CGRect?

    public let outputWidth: Int
    public let outputHeight: Int

    public let scale: CGFloat

    public init(
        outputWidth: Int,
        outputHeight: Int,
        scale: CGFloat = 1,
        allowsLooping: Bool = true,
        visibleRect: CGRect? = nil
    ) {
        self.outputWidth = outputWidth
        self.outputHeight = outputHeight
        self.scale = scale
        self.allowsLooping = allowsLooping
        self.visibleRect = visibleRect
    }
}
