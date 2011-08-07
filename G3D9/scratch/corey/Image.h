

#ifndef G3D_IMAGE_H
#define G3D_IMAGE_H

#include "FreeImagePlus.h"
#include "ImageBuffer.h"
#include "G3D/ReferenceCount.h"
#include "G3D/Vector2int32.h"


namespace G3D {

// Forward declarations
class ImageFormat;


class Image : public ReferenceCountedObject {
public:
    typedef ReferenceCountedPointer<Image> Ref;

private:
    fipImage            m_image;
    const ImageFormat*  m_format;


    Image();

    const ImageFormat* determineImageFormat() const;

public:
    virtual ~Image();

    static Ref fromFile(const std::string& filename, FREE_IMAGE_FORMAT fileFormat = FIF_UNKNOWN, const ImageFormat* imageFormat = ImageFormat::AUTO());
    static Ref fromInput(const BinaryInput& bi, FREE_IMAGE_FORMAT fileFormat = FIF_UNKNOWN, const ImageFormat* imageFormat = ImageFormat::AUTO());


    void get(const Point2int32& pos, Color4& color) const;

    void set(const Point2int32& pos, const Color4& color);
};

} // namespace G3D

#endif // G3D_IMAGE_h