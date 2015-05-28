#ifndef OSMSCOUT_IMPORT_GENAREAAREAINDEX_H
#define OSMSCOUT_IMPORT_GENAREAAREAINDEX_H

/*
  This source is part of the libosmscout library
  Copyright (C) 2010  Tim Teulings

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
*/

#include <map>
#include <set>

#include <osmscout/Pixel.h>

#include <osmscout/util/FileWriter.h>
#include <osmscout/util/Geometry.h>

#include <osmscout/import/Import.h>

namespace osmscout {

  class AreaAreaIndexGenerator : public ImportModule
  {
  private:
    struct Entry
    {
      TypeId     type;
      FileOffset offset;
    };

    struct AreaLeaf
    {
      FileOffset       offset;
      std::list<Entry> areas;
      FileOffset       children[4];

      AreaLeaf()
      {
        offset=0;
        children[0]=0;
        children[1]=0;
        children[2]=0;
        children[3]=0;
      }
    };

  private:
    void SetOffsetOfChildren(const std::map<Pixel,AreaLeaf>& leafs,
                             std::map<Pixel,AreaLeaf>& newAreaLeafs);

    bool WriteIndexLevel(const TypeConfigRef& typeConfig,
                         const ImportParameter& parameter,
                         FileWriter& writer,
                         size_t level,
                         std::map<Pixel,AreaLeaf>& leafs);


  public:
    std::string GetDescription() const;
    bool Import(const TypeConfigRef& typeConfig,
                const ImportParameter& parameter,
                Progress& progress);
  };
}

#endif
