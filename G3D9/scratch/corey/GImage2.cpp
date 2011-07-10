
#include "GImage2.h"


namespace G3D {

Image::Image() {
}

Image::~Image() {

}

Image::Ref Image::fromFile(const std::string& filename) {
    Image* img = new Image;

    if (! img->mImage.load(filename.c_str()))
    {
        delete img;
        img = NULL;
    }

    return img;
}



} // namespace G3D