import CoreGraphics
import Foundation
import UIKit

public struct Renderer {
    private let context: CGContext
    private let scale: CGFloat

    public init?(width: Int, height: Int, scale: CGFloat) {
        let bitmapInfo = CGBitmapInfo(
            rawValue: CGImageAlphaInfo.premultipliedFirst.rawValue
        ).union(.byteOrder32Little)

        let scale = scale == 0 ? UIScreen.main.scale : scale

        guard let context = CGContext(
            data: nil,
            width: Int(CGFloat(width) * scale),
            height: Int(CGFloat(height) * scale),
            bitsPerComponent: 8,
            bytesPerRow: 0,
            space: CGColorSpaceCreateDeviceRGB(),
            bitmapInfo: bitmapInfo.rawValue
        )
        else {
            return nil
        }

        self.context = context
        self.scale = scale
    }

    public func render(animation: Animation, frameIndex: Int, visibleRect: CGRect? = nil) -> UIImage? {
        animation.render(frameWithIndex: frameIndex, in: context)

        guard let cgImage = context.makeImage() else {
            return nil
        }

        if let visibleRect = visibleRect {
            return cgImage.croppingIfNeeded(to: visibleRect).map {
                UIImage(cgImage: $0, scale: scale, orientation: .up)
            }
        }
        else {
            return UIImage(cgImage: cgImage, scale: scale, orientation: .up)
        }
    }
}

private extension CGImage {
    func croppingIfNeeded(to rect: CGRect) -> CGImage? {
        guard rect.origin != .zero && rect.size != CGSize(width: width, height: height) else {
            return self
        }

        return self.cropping(to: rect)
    }
}
