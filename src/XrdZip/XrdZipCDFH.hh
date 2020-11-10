//------------------------------------------------------------------------------
// Copyright (c) 2011-2014 by European Organization for Nuclear Research (CERN)
// Author: Michal Simon <michal.simon@cern.ch>
//------------------------------------------------------------------------------
// This file is part of the XRootD software suite.
//
// XRootD is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// XRootD is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with XRootD.  If not, see <http://www.gnu.org/licenses/>.
//
// In applying this licence, CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.
//------------------------------------------------------------------------------

#ifndef SRC_XRDZIP_XRDZIPCDFH_HH_
#define SRC_XRDZIP_XRDZIPCDFH_HH_

#include "XrdZip/XrdZipLFH.hh"
#include "XrdZip/XrdZipUtils.hh"
#include "XrdZip/XrdZipError.hh"

#include <string>
#include <algorithm>
#include <iterator>

namespace XrdZip
{
  //---------------------------------------------------------------------------
  // A data structure representing the Central Directory File header record
  //---------------------------------------------------------------------------
  struct CDFH
  {
    //-------------------------------------------------------------------------
    // Constructor from Local File Header
    //-------------------------------------------------------------------------
    CDFH( LFH *lfh, mode_t mode, uint64_t lfhOffset ):
      zipVersion( ( 3 << 8 ) | 63 ),
      generalBitFlag( lfh->generalBitFlag ),
      compressionMethod( lfh->compressionMethod ),
      timestmp( lfh->timestmp ),
      ZCRC32( lfh->ZCRC32 ),
      compressedSize( lfh->compressedSize ),
      uncompressedSize( lfh->uncompressedSize ),
      filenameLength( lfh->filenameLength ),
      commentLength( 0 ),
      nbDisk( 0 ),
      internAttr( 0 ),
      externAttr( mode << 16 ),
      extra( new Extra( lfh->extra.get(), lfhOffset ) ),
      filename( lfh->filename )
    {
      if ( lfhOffset >= ovrflw<uint32_t>::value )
        offset = ovrflw<uint32_t>::value;
      else
        offset = lfhOffset;

      extraLength = extra->totalSize;

      if ( extraLength == 0 )
        minZipVersion = 10;
      else
        minZipVersion = 45;

      cdfhSize = cdfhBaseSize + filenameLength + extraLength + commentLength;
    }

    //-------------------------------------------------------------------------
    // Constructor from buffer
    //-------------------------------------------------------------------------
    CDFH( const char *buffer )
    {
      zipVersion        = *reinterpret_cast<const uint16_t*>( buffer + 4 );
      minZipVersion     = *reinterpret_cast<const uint16_t*>( buffer + 6 );
      generalBitFlag    = *reinterpret_cast<const uint16_t*>( buffer + 8 );
      compressionMethod = *reinterpret_cast<const uint16_t*>( buffer + 10 );
      timestmp.time     = *reinterpret_cast<const uint16_t*>( buffer + 12 );
      timestmp.date     = *reinterpret_cast<const uint16_t*>( buffer + 14 );
      ZCRC32            = *reinterpret_cast<const uint32_t*>( buffer + 16 );
      compressedSize    = *reinterpret_cast<const uint32_t*>( buffer + 20 );
      uncompressedSize  = *reinterpret_cast<const uint32_t*>( buffer + 24 );
      filenameLength    = *reinterpret_cast<const uint16_t*>( buffer + 28 );
      extraLength       = *reinterpret_cast<const uint16_t*>( buffer + 30 );
      commentLength     = *reinterpret_cast<const uint16_t*>( buffer + 32 );
      nbDisk            = *reinterpret_cast<const uint16_t*>( buffer + 34 );
      internAttr        = *reinterpret_cast<const uint16_t*>( buffer + 36 );
      externAttr        = *reinterpret_cast<const uint32_t*>( buffer + 38 );
      offset            = *reinterpret_cast<const uint32_t*>( buffer + 42 );

      filename.assign( buffer + 46, filenameLength );

      // now parse the 'extra' (may contain the zip64 extension to CDFH)
      ParseExtra( buffer + 46 + filenameLength, extraLength );

      cdfhSize = cdfhBaseSize + filenameLength + extraLength + commentLength;
    }

    //-------------------------------------------------------------------------
    // Parse the extensible data fields
    //-------------------------------------------------------------------------
    void ParseExtra( const char *buffer, uint16_t length)
    {
      uint8_t ovrflws = Extra::NONE;
      uint16_t exsize = 0;

      // check if compressed size is overflown
      if( compressedSize == ovrflw<uint32_t>::value)
      {
        ovrflws |= Extra::CPMSIZE;
        exsize  += sizeof( uint64_t );
      }

      // check if original size is overflown
      if( uncompressedSize == ovrflw<uint32_t>::value )
      {
        ovrflws |= Extra::UCMPSIZE;
        exsize  += sizeof( uint64_t );
      }

      // check if offset is overflown
      if( offset == ovrflw<uint32_t>::value )
      {
        ovrflws |= Extra::OFFSET;
        exsize  += sizeof( uint64_t );
      }

      // check if number of disks is overflown
      if( nbDisk == ovrflw<uint16_t>::value )
      {
        ovrflws |= Extra::NBDISK;
        exsize  += sizeof( uint32_t );
      }

      // if the expected size of ZIP64 extension is 0 we
      // can skip parsing of 'extra'
      if( exsize == 0 ) return;

      extra.reset( new Extra() );

      // Parse the extra part
      const char *end = buffer + length;
      while( buffer < end )
      {
        if( extra->FromBuffer( buffer, exsize, ovrflws ) )
          break;
      }
    }

    //-------------------------------------------------------------------------
    //! Serialize the object into a buffer
    //-------------------------------------------------------------------------
    void Serialize( buffer_t &buffer )
    {
      uint16_t size = cdfhSize - extraLength - commentLength;
      copy_bytes( cdfhSign, buffer );
      copy_bytes( zipVersion, buffer );
      copy_bytes( minZipVersion, buffer );
      copy_bytes( generalBitFlag, buffer );
      copy_bytes( compressionMethod, buffer );
      copy_bytes( timestmp.time, buffer );
      copy_bytes( timestmp.date, buffer );
      copy_bytes( ZCRC32, buffer );
      copy_bytes( compressedSize, buffer );
      copy_bytes( uncompressedSize, buffer );
      copy_bytes( filenameLength, buffer );
      copy_bytes( extraLength, buffer );
      copy_bytes( commentLength, buffer );
      copy_bytes( nbDisk, buffer );
      copy_bytes( internAttr, buffer );
      copy_bytes( externAttr, buffer );
      copy_bytes( offset, buffer );
      std::copy( filename.begin(), filename.end(), std::back_inserter( buffer ) );
      if( extra )
        extra->Serialize( buffer );

      if ( commentLength > 0 )
        std::copy( comment.begin(), comment.end(), std::back_inserter( buffer ) );
    }

    uint16_t                zipVersion;        // ZIP version
    uint16_t                minZipVersion;     //< minumum ZIP version
    uint16_t                generalBitFlag;    //< flags
    uint16_t                compressionMethod; //< compression method
    dos_timestmp            timestmp;          //< DOS timestamp
    uint32_t                ZCRC32;            //< CRC32
    uint32_t                compressedSize;    //< compressed size
    uint32_t                uncompressedSize;  //< uncompressed size
    uint16_t                filenameLength;    //< filename length
    uint16_t                extraLength;       //< size of the ZIP64 extra field
    uint16_t                commentLength;     //< comment length
    uint16_t                nbDisk;            //< number of disks
    uint16_t                internAttr;        //< internal attributes
    uint32_t                externAttr;        //< external attributes
    uint32_t                offset;            //< offset
    std::string             filename;          //< file name
    std::unique_ptr<Extra>  extra;             //< ZIP64 extra field
    std::string             comment;           //< user comment
    uint16_t                cdfhSize;          // size of the record

    //-------------------------------------------------------------------------
    // the Central Directory File Header signature
    //-------------------------------------------------------------------------
    static const uint32_t cdfhSign = 0x02014b50;
    static const uint16_t cdfhBaseSize = 46;
  };
}

#endif /* SRC_XRDZIP_XRDZIPCDFH_HH_ */
