/*
  This source is part of the libosmscout library
  Copyright (C) 2013  Tim Teulings

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

#include <osmscout/import/SortWayDat.h>

#include <osmscout/TypeFeatures.h>

#include <osmscout/util/Geometry.h>

namespace osmscout {

  class WayLocationProcessorFilter : public SortDataGenerator<Way>::ProcessingFilter
  {
  private:
    FileWriter                 writer;
    uint32_t                   overallDataCount;
    NameFeatureValueReader     *nameReader;
    LocationFeatureValueReader *locationReader;

  public:
    bool BeforeProcessingStart(const ImportParameter& parameter,
                               Progress& progress,
                               const TypeConfig& typeConfig);
    bool Process(Progress& progress,
                 const FileOffset& offset,
                 Way& way,
                 bool& save);
    bool AfterProcessingEnd(const ImportParameter& parameter,
                            Progress& progress,
                            const TypeConfig& typeConfig);
  };

  bool WayLocationProcessorFilter::BeforeProcessingStart(const ImportParameter& parameter,
                                                         Progress& progress,
                                                         const TypeConfig& typeConfig)
  {
    overallDataCount=0;

    nameReader=new NameFeatureValueReader(typeConfig);
    locationReader=new LocationFeatureValueReader(typeConfig);

    if (!writer.Open(AppendFileToDir(parameter.GetDestinationDirectory(),
                                     "wayaddress.dat"))) {
      progress.Error(std::string("Cannot create '")+writer.GetFilename()+"'");

      return false;
    }

    writer.Write(overallDataCount);

    return true;
  }

  bool WayLocationProcessorFilter::Process(Progress& /*progress*/,
                                           const FileOffset& offset,
                                           Way& way,
                                           bool& /*save*/)
  {
    if (!way.GetType()->GetIndexAsPOI()) {
      return true;
    }

    NameFeatureValue     *nameValue=nameReader->GetValue(way.GetFeatureValueBuffer());

    if (nameValue==NULL) {
      return true;
    }

    LocationFeatureValue *locationValue=locationReader->GetValue(way.GetFeatureValueBuffer());
    std::string          name;
    std::string          location;
    std::string          address;

    name=nameValue->GetName();

    if (locationValue!=NULL) {
      location=locationValue->GetLocation();
    }

    if (!writer.WriteFileOffset(offset)) {
      return false;
    }

    if (!writer.WriteNumber(way.GetType()->GetWayId())) {
      return false;
    }

    if (!writer.Write(name)) {
      return false;
    }

    if (!writer.Write(location)) {
      return false;
    }

    if (!writer.Write(way.nodes)) {
      return false;
    }

    overallDataCount++;

    return true;
  }

  bool WayLocationProcessorFilter::AfterProcessingEnd(const ImportParameter& /*parameter*/,
                                                      Progress& /*progress*/,
                                                      const TypeConfig& /*typeConfig*/)
  {
    delete nameReader;
    nameReader=NULL;

    delete locationReader;
    locationReader=NULL;

    writer.SetPos(0);
    writer.Write(overallDataCount);

    return writer.Close();
  }

  class WayNodeReductionProcessorFilter : public SortDataGenerator<Way>::ProcessingFilter
  {
  private:
    std::vector<GeoCoord> nodeBuffer;
    std::vector<Id>       idBuffer;
    size_t                duplicateCount;
    size_t                redundantCount;
    size_t                overallCount;

  private:
    bool IsEqual(const unsigned char buffer1[],
                 const unsigned char buffer2[]);

    bool RemoveDuplicateNodes(Progress& progress,
                              const FileOffset& offset,
                              Way& way,
                              bool& save);

    bool RemoveRedundantNodes(Progress& progress,
                              const FileOffset& offset,
                              Way& way,
                              bool& save);

  public:
    bool BeforeProcessingStart(const ImportParameter& parameter,
                               Progress& progress,
                               const TypeConfig& typeConfig);

    bool Process(Progress& progress,
                 const FileOffset& offset,
                 Way& way,
                 bool& save);

    bool AfterProcessingEnd(const ImportParameter& parameter,
                            Progress& progress,
                            const TypeConfig& typeConfig);
  };

  bool WayNodeReductionProcessorFilter::BeforeProcessingStart(const ImportParameter& /*parameter*/,
                                                              Progress& /*progress*/,
                                                              const TypeConfig& /*typeConfig*/)
  {
    duplicateCount=0;
    redundantCount=0;
    overallCount=0;

    return true;
  }

  bool WayNodeReductionProcessorFilter::IsEqual(const unsigned char buffer1[],
                                                const unsigned char buffer2[])
  {
    for (size_t i=0; i<coordByteSize; i++) {
      if (buffer1[i]!=buffer2[i]) {
        return false;
      }
    }

    return true;
  }

  bool WayNodeReductionProcessorFilter::RemoveDuplicateNodes(Progress& progress,
                                                             const FileOffset& offset,
                                                             Way& way,
                                                             bool& save)
  {
    unsigned char buffers[2][coordByteSize];

    bool reduced=false;

    if (way.nodes.size()>=2) {
      size_t lastIndex=0;
      size_t currentIndex=1;

      nodeBuffer.clear();
      idBuffer.clear();

      // Prefill with the first coordinate
      way.nodes[0].EncodeToBuffer(buffers[0]);

      nodeBuffer.push_back(way.nodes[0]);
      if (!way.ids.empty()) {
        idBuffer.push_back(way.ids[0]);
      }

      for (size_t n=1; n<way.nodes.size(); n++) {
        way.nodes[n].EncodeToBuffer(buffers[currentIndex]);

        if (IsEqual(buffers[lastIndex],
                    buffers[currentIndex])) {
          if (n>=way.ids.size() ||
              way.ids[n]==0) {
            duplicateCount++;
            reduced=true;
          }
          else if ((n-1)>=way.ids.size() ||
              way.ids[n-1]==0) {
            way.ids[n-1]=way.ids[n];
            duplicateCount++;
            reduced=true;
          }
          else {
            nodeBuffer.push_back(way.nodes[n]);
            if (n<way.ids.size()) {
              idBuffer.push_back(way.ids[n]);
            }

            lastIndex=currentIndex;
            currentIndex=(lastIndex+1)%2;
          }
        }
        else {
          nodeBuffer.push_back(way.nodes[n]);
          if (n<way.ids.size()) {
            idBuffer.push_back(way.ids[n]);
          }

          lastIndex=currentIndex;
          currentIndex=(lastIndex+1)%2;
        }
      }
    }

    if (reduced) {
      if (nodeBuffer.size()<2) {
        progress.Debug("Way " + NumberToString(offset) + " empty/invalid after node reduction");
        save=false;
        return true;
      }
      else {
        way.nodes=nodeBuffer;
        way.ids=idBuffer;
      }
    }

    return true;
  }

  bool WayNodeReductionProcessorFilter::RemoveRedundantNodes(Progress& /*progress*/,
                                                             const FileOffset& /*offset*/,
                                                             Way& way,
                                                             bool& /*save*/)
  {
    // In this case there is nothing to optimize
    if (way.nodes.size()<3) {
      return true;
    }

    nodeBuffer.clear();
    idBuffer.clear();

    size_t last=0;
    size_t current=1;
    bool   reduced=false;

    nodeBuffer.push_back(way.nodes[0]);
    if (!way.ids.empty()) {
      idBuffer.push_back(way.ids[0]);
    }

    while (current+1<way.nodes.size()) {
      double distance=CalculateDistancePointToLineSegment(way.nodes[current],
                                                          nodeBuffer[last],
                                                          way.nodes[current+1]);

      if (distance<1/latConversionFactor &&
          (current>=way.ids.size() || way.ids[current]==0)) {
        reduced=true;
        redundantCount++;
        current++;
      }
      else {
        nodeBuffer.push_back(way.nodes[current]);
        if (!way.ids.empty()) {
          idBuffer.push_back(way.ids[current]);
        }

        last++;
        current++;
      }
    }

    nodeBuffer.push_back(way.nodes[current]);
    if (!way.ids.empty()) {
      idBuffer.push_back(way.ids[current]);
    }

    if (reduced) {
      way.nodes=nodeBuffer;
      way.ids=idBuffer;
    }

    return true;
  }

  bool WayNodeReductionProcessorFilter::Process(Progress& progress,
                                                const FileOffset& offset,
                                                Way& way,
                                                bool& save)
  {
    overallCount+=way.nodes.size();

    if (!RemoveDuplicateNodes(progress,
                              offset,
                              way,
                              save)) {
      return false;
    }

    if (!RemoveRedundantNodes(progress,
                              offset,
                              way,
                              save)) {
      return false;
    }

    return true;
  }

  bool WayNodeReductionProcessorFilter::AfterProcessingEnd(const ImportParameter& /*parameter*/,
                                                           Progress& progress,
                                                           const TypeConfig& /*typeConfig*/)
  {
    progress.Info("Duplicate nodes removed: " + NumberToString(duplicateCount));
    progress.Info("Redundant nodes removed: " + NumberToString(redundantCount));
    progress.Info("Overall nodes: " + NumberToString(overallCount));

    return true;
  }

  void SortWayDataGenerator::GetTopLeftCoordinate(const Way& data,
                                                  double& maxLat,
                                                  double& minLon)
  {
    maxLat=data.nodes[0].GetLat();
    minLon=data.nodes[0].GetLon();

    for (size_t n=1; n<data.nodes.size(); n++) {
      maxLat=std::max(maxLat,data.nodes[n].GetLat());
      minLon=std::min(minLon,data.nodes[n].GetLon());
    }
  }

  SortWayDataGenerator::SortWayDataGenerator()
  : SortDataGenerator<Way>("ways.dat","ways.idmap")
  {
    AddSource("ways.tmp");

    AddFilter(new WayLocationProcessorFilter());
    AddFilter(new WayNodeReductionProcessorFilter());
  }

  std::string SortWayDataGenerator::GetDescription() const
  {
    return "Sort/copy ways";
  }

}
