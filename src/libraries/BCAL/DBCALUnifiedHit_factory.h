#ifndef _DBCALUnifiedHit_factory_
#define _DBCALUnifiedHit_factory_

#include <JANA/JFactory.h>
#include <JANA/JEventLoop.h>

using namespace jana;

#include "BCAL/DBCALUnifiedHit.h"
#include "BCAL/DBCALTDCHit.h"
#include "BCAL/DBCALHit.h"

#include <TTree.h>

class DBCALUnifiedHit_factory : public JFactory<DBCALUnifiedHit> {

 public:

  DBCALUnifiedHit_factory() {}
  ~DBCALUnifiedHit_factory() {}

  TTree *bcal_points_tree;

 private:
 
  class cellHits{
   public:
    vector<const DBCALHit*> hits;
    vector<const DBCALTDCHit*> tdc_hits;
  };
 
  jerror_t init(void);
  jerror_t brun(jana::JEventLoop *eventLoop, int runnumber); ///< Called everytime a new run number is detected.
  jerror_t evnt(JEventLoop *loop, int eventnumber);

  float E_tree;
  float t_tdc_tree;
  float t_adc_tree;
  int layer_tree;
  bool end_tree;

  //Used as a key for maps
  class readout_channel {
   public:
    readout_channel(int cellId, DBCALGeometry::End end) :
      cellId(cellId), end(end) {}

    int cellId;
    DBCALGeometry::End end;

    bool operator<(const readout_channel &c) const {
      if (cellId<c.cellId) return true;
      if (cellId>c.cellId) return false;
      if (end==DBCALGeometry::kUpstream && c.end==DBCALGeometry::kDownstream) return true;
      return false;
    }
  };

  //For now timewalk corrections are of the form f(ADC) = c0 + c1/(ADC-c3)^c2
  //Store all coefficients in one structure
  class timewalk_coefficients {
   public:
    timewalk_coefficients() :
      c0(0), c1(0), c2(0), c3(0) {}
    timewalk_coefficients(float c0, float c1, float c2, float c3) :
      c0(c0), c1(c1), c2(c2), c3(c3) {}
    float c0,c1,c2,c3;
  };

  map<readout_channel,timewalk_coefficients> adc_timewalk_map;
  map<readout_channel,timewalk_coefficients> tdc_timewalk_map;
};

#endif //_DBCALUnifiedHit_factory_
