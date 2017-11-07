#ifndef VISUALCLOUD_PHYSICAL_H
#define VISUALCLOUD_PHYSICAL_H

#include "LightField.h"
#include "Encoding.h"
#include "TileVideoEncoder.h" //TODO can remove this after hacks addressed
#include <tuple>
#include <stdexcept>

namespace visualcloud {
    namespace physical {

        template<typename ColorSpace>
        class EquirectangularTiledLightField: public LightField<ColorSpace> {
        public:
            EquirectangularTiledLightField(LightFieldReference<ColorSpace> &field)
                : EquirectangularTiledLightField(field_, get_dimensions(&*field))
            { }

            const std::vector<LightFieldReference<ColorSpace>> provenance() const override { return {field_}; }
            const ColorSpace colorSpace() const override { return ColorSpace::Instance; }
            const std::vector<Volume> volumes() const override { return field_->volumes(); }
            const unsigned int rows() const { return rows_; }
            const unsigned int columns() const { return columns_; }
            inline const typename ColorSpace::Color value(const Point6D &point) const override { return field_->value(point); }

            EncodedLightField apply(const std::string&);

            static void hardcode_hack(const unsigned int framerate, const unsigned int gop, const unsigned int height, const unsigned int width, const unsigned int rows, const unsigned int columns, const unsigned int max_bitrate, const std::string &intermediate_format, const std::string &output_format);
            //TODO hacks...
            static double hack_divide(const double left, const visualcloud::rational &right) {
                //TODO oh noes...
                return left / ((double)right.numerator() / (double)right.denominator()) + 0.5;
            }
            static unsigned int gop, max_bitrate; //framerate, height, width; //, rows, columns;
            static std::shared_ptr<EncodeConfiguration> encodeConfiguration;
            static std::shared_ptr<DecodeConfiguration> decodeConfiguration;
            static std::shared_ptr<GPUContext> context;
            static std::shared_ptr<TileVideoEncoder> tiler;
            static std::string decode_format, encode_format;
            static bool executed;
            //GPUContext context_; //TODO


        private:
            using metadata = std::tuple<size_t, size_t, size_t, PanoramicVideoLightField<EquirectangularGeometry, ColorSpace>&>;

            EquirectangularTiledLightField(LightFieldReference<ColorSpace> &field, const metadata data)
                : field_(field), rows_(std::get<0>(data)), columns_(std::get<1>(data)), time_(std::get<2>(data)), video_((std::get<3>(data)))
                  //context_(0) //TODO context
            { }

            static metadata get_dimensions(LightField<ColorSpace>* field, size_t rows=1, size_t columns=1, size_t time=0) {
                auto *partitioner = dynamic_cast<const PartitionedLightField<ColorSpace>*>(field);
                auto *video = dynamic_cast<PanoramicVideoLightField<EquirectangularGeometry, ColorSpace>*>(field);
                auto *child = field->provenance().size() == 1 ? &*field->provenance().at(0) : nullptr;
                // Don't cast for every prohibited type
                auto *discrete = dynamic_cast<DiscreteLightField<ColorSpace>*>(field);

                if(video != nullptr) {
                    if(rows > 1 || columns > 1)
                        return {rows, columns, time, *video};
                    else {
                        LOG(WARNING) << "Attempt to perform 1x1 tiling; use transcode instead";
                        throw std::invalid_argument("Attempt to perform 1x1 tiling; ignore (or use transcode if format changed) instead");
                    }
                } else if(partitioner != nullptr && partitioner->dimension() == Dimension::Theta)
                    return get_dimensions(child, rows, hack_divide(AngularRange::ThetaMax.end, partitioner->interval()), time);
                else if(partitioner != nullptr && partitioner->dimension() == Dimension::Phi)
                    return get_dimensions(child, hack_divide(AngularRange::PhiMax.end, partitioner->interval()), columns, time);
                else if(partitioner != nullptr && partitioner->dimension() == Dimension::Time)
                    return get_dimensions(child, rows, columns, (double)partitioner->interval().numerator() / partitioner->interval().denominator());
                // TODO prohibit other intermediating field types...
                // TODO volume may not be pointwise spatial...
                else if(child != nullptr && discrete == nullptr)
                    return get_dimensions(child, rows, columns, time);
                else {
                    LOG(WARNING) << "Attempt to tile field not backed by logical PanoramicVideoLightField";
                    throw std::invalid_argument("Query not backed by logical PanoramicVideoLightField");
                }
            }
 
            LightFieldReference<ColorSpace> field_;
            const unsigned int rows_, columns_;
            const double time_;
            const PanoramicVideoLightField<EquirectangularGeometry, ColorSpace>& video_;
        };

        template<typename ColorSpace>
        class StitchedLightField: public LightField<ColorSpace> {
        public:
            StitchedLightField(const LightFieldReference<ColorSpace> &field)
                    : StitchedLightField(field, get_tiles(field))
            { }

            const std::vector<LightFieldReference<ColorSpace>> provenance() const override { return {field_}; }
            const ColorSpace colorSpace() const override { return ColorSpace::Instance; }
            const std::vector<Volume> volumes() const override { return field_->volumes(); }
            inline const typename ColorSpace::Color value(const Point6D &point) const override { return field_->value(point); }

            EncodedLightField apply();
            EncodedLightField apply(const visualcloud::rational &temporalInterval);

        private:
            StitchedLightField(const LightFieldReference<ColorSpace> &field,
                               const std::pair<std::vector<PanoramicVideoLightField<EquirectangularGeometry, ColorSpace>*>, std::vector<Volume>> &pair)
                    : field_(field), videos_(pair.first), volumes_(pair.second)
            { }

            static std::pair<std::vector<PanoramicVideoLightField<EquirectangularGeometry, ColorSpace>*>, std::vector<Volume>> get_tiles(const LightFieldReference<ColorSpace>& field) {
                auto *composite = dynamic_cast<const CompositeLightField<ColorSpace>*>(&*field);
                std::vector<PanoramicVideoLightField<EquirectangularGeometry, ColorSpace>*> videos;
                std::vector<Volume> volumes;

                if(composite == nullptr)
                    throw std::invalid_argument("Plan root was not a composite.");

                for(auto &child: field->provenance()) {
                    LightField<ColorSpace> *cf = &*child;
                    auto *video = dynamic_cast<PanoramicVideoLightField<EquirectangularGeometry, ColorSpace>*>(cf);
                    auto *rotation = dynamic_cast<RotatedLightField<ColorSpace>*>(cf);
                    if(rotation != nullptr)
                        video = dynamic_cast<PanoramicVideoLightField<EquirectangularGeometry, ColorSpace>*>(&*rotation->provenance()[0]);

                    if(video == nullptr)
                        throw std::invalid_argument("Composite child was not a video.");
                    else if(video->metadata().codec != "hevc")
                        throw std::invalid_argument("Input video was not HEVC encoded.");

                    videos.push_back(video);
                    volumes.push_back(rotation != nullptr ? rotation->volumes()[0] : video->volumes()[0]);
                }

                return std::make_pair(videos, volumes);
            }

            const LightFieldReference<ColorSpace> field_;
            const std::vector<PanoramicVideoLightField<EquirectangularGeometry, ColorSpace>*> videos_;
            const std::vector<Volume> volumes_;
        };

        template<typename ColorSpace>
        class NaiveStitchedLightField: public LightField<ColorSpace> {
        public:
            NaiveStitchedLightField(const LightFieldReference<ColorSpace> &field)
                    : NaiveStitchedLightField(field, get_tiles(field))
            { }

            const std::vector<LightFieldReference<ColorSpace>> provenance() const override { return {field_}; }
            const ColorSpace colorSpace() const override { return ColorSpace::Instance; }
            const std::vector<Volume> volumes() const override { return field_->volumes(); }
            inline const typename ColorSpace::Color value(const Point6D &point) const override { return field_->value(point); }

            EncodedLightField apply();

        private:
            NaiveStitchedLightField(const LightFieldReference<ColorSpace> &field,
                                    const std::pair<std::vector<PanoramicVideoLightField<EquirectangularGeometry, ColorSpace>*>,
                                            std::vector<Volume>> &pair)
                    : field_(field), videos_(pair.first), volumes_(pair.second)
            { }

            static std::pair<std::vector<PanoramicVideoLightField<EquirectangularGeometry, ColorSpace>*>, std::vector<Volume>> get_tiles(const LightFieldReference<ColorSpace>& field) {
                auto *composite = dynamic_cast<const CompositeLightField<ColorSpace>*>(&*field);
                std::vector<PanoramicVideoLightField<EquirectangularGeometry, ColorSpace>*> videos;
                std::vector<Volume> volumes;

                if(composite == nullptr)
                    throw std::invalid_argument("Plan root was not a composite.");

                for(auto &child: field->provenance()) {
                    LightField<ColorSpace> *cf = &*child;
                    auto *video = dynamic_cast<PanoramicVideoLightField<EquirectangularGeometry, ColorSpace>*>(cf);
                    auto *rotation = dynamic_cast<RotatedLightField<ColorSpace>*>(cf);
                    if(rotation != nullptr)
                        video = dynamic_cast<PanoramicVideoLightField<EquirectangularGeometry, ColorSpace>*>(&*rotation->provenance()[0]);

                    if(video == nullptr)
                        throw std::invalid_argument("Composite child was not a video.");

                    videos.push_back(video);
                    volumes.push_back(rotation != nullptr ? rotation->volumes()[0] : video->volumes()[0]);
                }

                return std::make_pair(videos, volumes);
            }

            const LightFieldReference<ColorSpace> field_;
            const std::vector<PanoramicVideoLightField<EquirectangularGeometry, ColorSpace>*> videos_;
            const std::vector<Volume> volumes_;
        };

        template<typename ColorSpace>
        class EquirectangularCroppedLightField: public LightField<ColorSpace> {
        public:
            EquirectangularCroppedLightField(const PanoramicVideoLightField<EquirectangularGeometry, ColorSpace> &video, AngularRange theta, AngularRange phi)
                    : video_(video), theta_(theta), phi_(phi)
            { }

            const std::vector<LightFieldReference<ColorSpace>> provenance() const override { return {}; } //TODO incorrect
            const ColorSpace colorSpace() const override { return ColorSpace::Instance; }
            const std::vector<Volume> volumes() const override { return video_.volumes(); }
            inline const typename ColorSpace::Color value(const Point6D &point) const override { return video_.value(point); }

            EncodedLightField apply(const std::string &format);

        private:
            const PanoramicVideoLightField<EquirectangularGeometry, ColorSpace>& video_;
            const AngularRange theta_, phi_;
        };

        template<typename ColorSpace>
        class EquirectangularTranscodedLightField: public LightField<ColorSpace> {
        public:
            EquirectangularTranscodedLightField(const PanoramicVideoLightField<EquirectangularGeometry, ColorSpace> &video,
                                                const visualcloud::functor<ColorSpace> &functor)
                    : video_(video), functor_(functor)
            { }

            const std::vector<LightFieldReference<ColorSpace>> provenance() const override { video_.provenance(); } //TODO incorrect
            const ColorSpace colorSpace() const override { return ColorSpace::Instance; }
            const std::vector<Volume> volumes() const override { return video_.volumes(); }
            inline const typename ColorSpace::Color value(const Point6D &point) const override {
                return functor_(video_, point);
            }

            EncodedLightField apply(const std::string &format);

        private:
            const PanoramicVideoLightField<EquirectangularGeometry, ColorSpace>& video_;
            const visualcloud::functor<ColorSpace> &functor_;
        };

        //TODO hacks
        //template<typename ColorSpace> unsigned int EquirectangularTiledLightField<ColorSpace>::framerate = 0;
        template<typename ColorSpace> unsigned int EquirectangularTiledLightField<ColorSpace>::gop = 0;
        //template<typename ColorSpace> unsigned int EquirectangularTiledLightField<ColorSpace>::height = 0;
        //template<typename ColorSpace> unsigned int EquirectangularTiledLightField<ColorSpace>::width = 0;
        template<typename ColorSpace> unsigned int EquirectangularTiledLightField<ColorSpace>::max_bitrate = 0;
        //template<typename ColorSpace> unsigned int EquirectangularTiledLightField<ColorSpace>::rows = 0;
        //template<typename ColorSpace> unsigned int EquirectangularTiledLightField<ColorSpace>::columns = 0;
        template<typename ColorSpace> bool EquirectangularTiledLightField<ColorSpace>::executed = false;
        template<typename ColorSpace> std::shared_ptr<EncodeConfiguration> EquirectangularTiledLightField<ColorSpace>::encodeConfiguration = nullptr;
        template<typename ColorSpace> std::shared_ptr<DecodeConfiguration> EquirectangularTiledLightField<ColorSpace>::decodeConfiguration = nullptr;
        template<typename ColorSpace> std::shared_ptr<GPUContext> EquirectangularTiledLightField<ColorSpace>::context = nullptr;
        template<typename ColorSpace> std::shared_ptr<TileVideoEncoder> EquirectangularTiledLightField<ColorSpace>::tiler = nullptr;
        template<typename ColorSpace> std::string EquirectangularTiledLightField<ColorSpace>::encode_format = "";
        template<typename ColorSpace> std::string EquirectangularTiledLightField<ColorSpace>::decode_format = "";
    } // namespace physical
} // namespace visualcloud

#endif //VISUALCLOUD_PHYSICAL_H
