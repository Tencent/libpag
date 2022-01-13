#ifndef PAG_SOFTWARE_DECODER_H
#define PAG_SOFTWARE_DECODER_H

#include <vector>
#include <string>
#include <memory>

namespace pag {
    struct OutputFrame {
        /**
         * The YUV data, each value is a pointer to the picture planes.
         */
        uint8_t* data[3];
        /**
         * The size in bytes of each picture line.
         */
        int lineSize[3];
    };

    struct CodecHeader {
        /**
         * Codec specific data
         */
        uint8_t* data;
        /**
        * The size in bytes of each CSD.
        */
        size_t length;
    };

    enum class SoftwareDecodingResult {
        Success = 0,            //success
        TryAgainLater = -1,     //try again later.
        Error = -2,             //error
    };

    class SoftwareDecoder;

    /**
     * The factory of software decoder.
     */
    class SoftwareDecoderFactory {
    public:
        virtual ~SoftwareDecoderFactory() = default;

        /**
         * Create a software decoder
         */
        virtual std::unique_ptr<SoftwareDecoder> createSoftwareDecoder() = 0;
    };

    class SoftwareDecoder {
    public:
        virtual ~SoftwareDecoder()  = default;

        /**
         * Configure the software decoder.
         * @param headers Codec specific data. for example: csd-0、csd-1.
         * @param mime MIME type. for example: "video/avc"
         * @param width video width
         * @param height video height
         * @return Return true if configure successfully.
         */
        virtual bool onConfigure(const std::vector<CodecHeader>& headers, std::string mime,
                                 int width, int height) = 0;

        /**
         * Send a frame of bytes for decoding. The same bytes will be sent next time if it returns
         * SoftwareDecodeResult::TryAgainLater
         * @param bytes The sample data for decoding.
         * @param length The size of sample data
         * @param frame The timestamp of this sample data.
         */
        virtual SoftwareDecodingResult onSendBytes(void* bytes, size_t length, int64_t frame) = 0;

        /**
         * Try to decode a new frame from the pending frames sent by onSendBytes(). More pending
         * frames will be sent by onSendBytes() if it returns SoftwareDecodeResult::TryAgainLater.
         */
        virtual SoftwareDecodingResult onDecodeFrame() = 0;
        
        /**
         * Called to notify there is no more sample bytes available.
         */
        virtual SoftwareDecodingResult onEndOfStream() = 0;
        
        /**
         * Called when seeking happens to clear all pending frames.
         */
        virtual void onFlush() = 0;

        /**
         * Return decoded data to render, the format of decoded data must be in YUV420p format.
         */
        virtual std::unique_ptr<OutputFrame> onRenderFrame() = 0;

        
    };
}

#endif
