///////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2021 Tencent. All rights reserved.
//
//  This library is free software; you can redistribute it and/or modify it under the terms of the
//  GNU Lesser General Public License as published by the Free Software Foundation; either
//  version 2.1 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
//  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
//  the GNU Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public License along with this
//  library; if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
//  Boston, MA  02110-1301  USA
//
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#ifdef _WIN32
#define FFAVC_EXPORT __declspec(dllexport)
#else
#define FFAVC_EXPORT __attribute__((visibility("default")))
#endif

namespace ffavc {
class FFAVC_EXPORT DecoderFactory {
public:
  /**
   * Returns a handle of the decoder factory which implements the pag::SoftwareDecoderFactory API.
   * The returned handle can be type-casted to a pag::SoftwareDecoderFactory pointer, then passed in
   * the decoderFactory parameter of pag::PAGVideoDecoder::RegisterSoftwareDecoderFactory().
   */
  static void* GetHandle();
};
}  // namespace ffavc
