#include "native/CatalogPreviewProbe.h"
#include <iostream>
#include <stdexcept>
using namespace stui;
int main() {
 try {
  if(native::catalogPreviewNodeName(core::TattooArea::body,0)!="Body [Ovl0]" ||
     native::catalogPreviewNodeName(core::TattooArea::face,3)!="Face [Ovl3]" ||
     native::catalogPreviewNodeName(core::TattooArea::hands,2)!="Hands [Ovl2]" ||
     native::catalogPreviewNodeName(core::TattooArea::feet,1)!="Feet [Ovl1]" ||
     native::catalogPreviewNodeName(core::TattooArea::body,-1) ||
     native::catalogPreviewNodeName(static_cast<core::TattooArea>(99),0))
   throw std::runtime_error("exact normal overlay node naming");
  using Reason=native::CatalogPreviewMaterialSupport;
  if(native::catalogPreviewMaterialSupport({})!=Reason::missingGeometry ||
     native::catalogPreviewMaterialSupport({true,false,false,false})!=Reason::unsupportedShader ||
     native::catalogPreviewMaterialSupport({true,true,false,true})!=Reason::tintConversionRequired ||
     native::catalogPreviewMaterialSupport({true,true,true,false})!=Reason::missingEmissionStorage ||
     native::catalogPreviewMaterialSupport({true,true,true,true})!=Reason::scalarCandidate)
   throw std::runtime_error("unsafe material must be rejected before shader property getters");
  core::TattooSlots sample{.actorFormId=42,.area=core::TattooArea::body};
  sample.slots={{.index=0,.occupancy=core::SlotOccupancy::empty},{.index=3,.occupancy=core::SlotOccupancy::external}};
  const auto selected=native::externalProbeTargets(sample);
  if(selected.size()!=1 || selected[0].actorFormId!=42 || selected[0].nodeName!="Body [Ovl3]")
   throw std::runtime_error("external probe must select only the exact actor and external slot");
  using Comparison=native::ExternalProbeComparison;
  if(native::compareExternalPaths("textures/a.dds","textures/a.dds")!=Comparison::match ||
     native::compareExternalPaths("Textures/Actors/mark.DDS","textures\\actors\\mark.dds")!=Comparison::match ||
     native::compareExternalPaths("textures/a.dds","textures/b.dds")!=Comparison::mismatch ||
     native::compareExternalPaths(std::nullopt,"textures/a.dds")!=Comparison::unavailable ||
     native::compareExternalPaths("", "textures/a.dds")!=Comparison::unavailable)
   throw std::runtime_error("external texture comparison must distinguish missing, matching, and conflicting values");
  std::cout<<"PASS read-only catalog material capability classification\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
