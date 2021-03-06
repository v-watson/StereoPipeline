// __BEGIN_LICENSE__
// Copyright (C) 2006-2011 United States Government as represented by
// the Administrator of the National Aeronautics and Space Administration.
// All Rights Reserved.
// __END_LICENSE__


/// \file disparitydebug.cc
///

#ifdef _MSC_VER
#pragma warning(disable:4244)
#pragma warning(disable:4267)
#pragma warning(disable:4996)
#endif

#ifdef NDEBUG
#undef NDEBUG
#endif

#include <stdlib.h>

#include <vw/FileIO.h>
#include <vw/Image.h>
#include <vw/Stereo/DisparityMap.h>
#include <vw/tools/Common.h>
#include <asp/Core/Macros.h>
#include <asp/Core/Common.h>
using namespace vw;
using namespace vw::stereo;

namespace po = boost::program_options;
namespace fs = boost::filesystem;

struct Options : asp::BaseOptions {
  // Input
  std::string input_file_name;

  // Output
  std::string output_prefix, output_file_type;
};

void handle_arguments( int argc, char *argv[], Options& opt ) {
  po::options_description general_options("");
  general_options.add_options()
    ("output-prefix,o", po::value(&opt.output_prefix), "Specify the output prefix")
    ("output-filetype,t", po::value(&opt.output_file_type)->default_value("tif"), "Specify the output file");
  general_options.add( asp::BaseOptionsDescription(opt) );

  po::options_description positional("");
  positional.add_options()
    ("input-file", po::value(&opt.input_file_name), "Input disparity map");

  po::positional_options_description positional_desc;
  positional_desc.add("input-file", 1);

  std::string usage("[options] <input disparity map>");
  po::variables_map vm =
    asp::check_command_line( argc, argv, opt, general_options,
                             positional, positional_desc, usage );

  if ( opt.input_file_name.empty() )
    vw_throw( ArgumentErr() << "Missing input file!\n"
              << usage << general_options );
  if ( opt.output_prefix.empty() )
    opt.output_prefix = fs::path(opt.input_file_name).stem();
}

template <class PixelT>
void do_disparity_visualization(Options& opt) {
  DiskImageView<PixelT > disk_disparity_map(opt.input_file_name);

  vw_out() << "\t--> Computing disparity range \n";

  // We don't want to sample every pixel as the image might be very
  // large. Let's subsample the image so that it is rough 1000x1000
  // samples.
  float subsample_amt =
    float(disk_disparity_map.cols())*float(disk_disparity_map.rows()) / ( 1000.f * 1000.f );
  BBox2 disp_range =
    get_disparity_range(subsample(disk_disparity_map,
                                  subsample_amt > 1 ? subsample_amt : 1));

  vw_out() << "\t    Horizontal - [" << disp_range.min().x()
           << " " << disp_range.max().x() << "]    Vertical: ["
           << disp_range.min().y() << " " << disp_range.max().y() << "]\n";

  typedef typename PixelChannelType<PixelT>::type ChannelT;
  ImageViewRef<ChannelT> horizontal =
    apply_mask(copy_mask(clamp(normalize(select_channel(disk_disparity_map,0),
                                         disp_range.min().x(), disp_range.max().x(),
                                         ChannelRange<ChannelT>::min(),ChannelRange<ChannelT>::max())),
                         disk_disparity_map));
  ImageViewRef<ChannelT> vertical =
    apply_mask(copy_mask(clamp(normalize(select_channel(disk_disparity_map,1),
                                         disp_range.min().y(), disp_range.max().y(),
                                         ChannelRange<ChannelT>::min(),ChannelRange<ChannelT>::max())),
                         disk_disparity_map));

  vw_out() << "\t--> Saving disparity debug images\n";
  block_write_gdal_image( opt.output_prefix+"-H."+opt.output_file_type,
                          channel_cast_rescale<uint8>(horizontal),
                          opt, TerminalProgressCallback("asp","\t    Left  : "));
  block_write_gdal_image( opt.output_prefix + "-V." + opt.output_file_type,
                          channel_cast_rescale<uint8>(vertical),
                          opt, TerminalProgressCallback("asp","\t    Right : "));
}

int main( int argc, char *argv[] ) {

  Options opt;
  try {
    handle_arguments( argc, argv, opt );

    vw_out() << "Opening " << opt.input_file_name << "\n";
    ImageFormat fmt = tools::taste_image(opt.input_file_name);

    switch(fmt.pixel_format) {
    case VW_PIXEL_GENERIC_2_CHANNEL:
      switch (fmt.channel_type) {
      case VW_CHANNEL_INT32:
        do_disparity_visualization<Vector2i>(opt); break;
      default:
        do_disparity_visualization<Vector2f>(opt); break;
      } break;
    case VW_PIXEL_RGB:
    case VW_PIXEL_GENERIC_3_CHANNEL:
      switch (fmt.channel_type) {
      case VW_CHANNEL_INT32:
        do_disparity_visualization<PixelMask<Vector2i> >(opt); break;
      default:
        do_disparity_visualization<PixelMask<Vector2f> >(opt); break;
      } break;
    default:
      vw_throw( ArgumentErr() << "Unsupported pixel format. Expected GENERIC 2 or 3 CHANNEL image." );
    }

  } ASP_STANDARD_CATCHES;

  return 0;
}
