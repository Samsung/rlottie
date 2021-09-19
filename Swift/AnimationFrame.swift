import CoreMedia
import UIKit

public class AnimationFrame {
    public internal(set) var image: UIImage
    public internal(set) var presentationTime: CMTime
    public internal(set) var duration: CMTime

    public init(image: UIImage, presentationTime: CMTime, duration: CMTime) {
        self.image = image
        self.presentationTime = presentationTime
        self.duration = duration
    }
}
