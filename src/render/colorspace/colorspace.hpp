#pragma once
#include "../pass.hpp"
#include <vector>

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavutil/opt.h>
    #include <libavutil/imgutils.h>
    #include <libswscale/swscale.h>
    #include <libavfilter/avfilter.h>
    #include <libavfilter/buffersrc.h>
    #include <libavfilter/buffersink.h>
    #include <libswresample/swresample.h>
}

class Colorspace {
public:
    uint32_t m_alignedWidth, m_alignedHeight;

    virtual const std::vector<RenderPass> getPasses() = 0;
    virtual size_t getBufferSize() = 0;
    virtual geode::Result<> prepareFrame(AVFrame* frame, uint8_t* data, size_t size) = 0;

    virtual ~Colorspace() = default;
};