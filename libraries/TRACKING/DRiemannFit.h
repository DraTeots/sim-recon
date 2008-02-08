#ifndef _DRIEMANN_FIT_H_
#define _DRIEMANN_FIT_H_

#include <vector>
using namespace std;

#include <DMatrix.h>
#include "JANA/jerror.h"

typedef struct{
        double x,y,z;            ///< point in lab coordinates
        double covx,covy,covxy;  ///< error info for x and y coordinates
}DRiemannHit_t;

class DRiemannFit{
 public:
  DRiemannFit(){
    CovR_=NULL;
    CovRPhi_=NULL;
  };

  jerror_t DRiemannFit::FitCircle(double BeamRMS, double rc);
  jerror_t FitCircle(double BeamRMS);
  jerror_t FitLine(double BeamRMS);

  jerror_t AddHit(double r, double phi, double z);
  jerror_t AddHitXYZ(double x,double y, double z);
  jerror_t AddHit(double x,double y,double z,double covx,double covy,
		     double covxy);
 
  double GetCharge(double BeamRMS, double rc);
  double GetCharge(double BeamRMS);


  // Center of projected circle and radius
  double xc,yc,rc;
  // tangent of dip angle, vertex z position,z of reference plane
  double tanl, zvertex, z0;  
  double var_tanl;
  double p_trans;
  double phi;

 protected:
  jerror_t CalcNormal(DMatrix A,double lambda,DMatrix &N);

 private:
  vector<DRiemannHit_t*>hits;
  vector<DRiemannHit_t*>projections;
  DMatrix *CovR_;
  DMatrix *CovRPhi_;

  double N[3]; 
  double dist_to_origin;
};

#endif //_DRIEMANN_FIT_H_
























