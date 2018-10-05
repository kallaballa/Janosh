/*
 * compress.cpp
 *
 *  Created on: Oct 5, 2018
 *      Author: elchaschab
 */

#include "compress.hpp"
#include <sstream>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/zlib.hpp>

namespace janosh {

std::string compress_string(const std::string &data)
{
    std::stringstream compressed;
    std::stringstream original;
    original << data;
    boost::iostreams::filtering_streambuf<boost::iostreams::input> out;
    out.push(boost::iostreams::zlib_compressor());
    out.push(original);
    boost::iostreams::copy(out, compressed);
    return compressed.str();
}

std::string decompress_string(const std::string &data)
{
    std::stringstream compressed;
    std::stringstream decompressed;
    compressed << data;

    /** first decode  then decompress **/

    boost::iostreams::filtering_streambuf<boost::iostreams::input> in;
    in.push(boost::iostreams::zlib_decompressor());
    in.push(compressed);
    boost::iostreams::copy(in, decompressed);
    return decompressed.str();
}

} /* namespace janosh */
