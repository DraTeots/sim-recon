// $Id$
//
//    File: DParticleID.cc
// Created: Mon Feb 28 14:48:56 EST 2011
// Creator: staylor (on Linux ifarml1 2.6.18-128.el5 x86_64)
//

#include "DParticleID.h"
#include <TRACKING/DTrackFitter.h>
#include "FCAL/DFCALGeometry.h"


#define C_EFFECTIVE 15. // start counter light propagation speed
#define OUT_OF_TIME_CUT 200.

// Routine for sorting dEdx data
bool static DParticleID_dedx_cmp(DParticleID::dedx_t a,DParticleID::dedx_t b){
  return a.dEdx < b.dEdx;
}

// Routine for sorting hypotheses according to FOM
bool static DParticleID_hypothesis_cmp(const DTrackTimeBased *a,
				       const DTrackTimeBased *b){
  return (a->FOM>b->FOM);
}


//---------------------------------
// DParticleID    (Constructor)
//---------------------------------
DParticleID::DParticleID(JEventLoop *loop)
{
  
  DApplication* dapp = dynamic_cast<DApplication*>(loop->GetJApplication());
  if(!dapp){
    _DBG_<<"Cannot get DApplication from JEventLoop! (are you using a JApplication based program?)"<<endl;
		return;
  }
  RootGeom = dapp->GetRootGeom();
  bfield = dapp->GetBfield(); 
  stepper= new DMagneticFieldStepper(bfield);

  // Get material properties for chamber gas
  double rho_Z_over_A_LnI=0,radlen=0;
  RootGeom->FindMat("CDchamberGas",mRhoZoverAGas,rho_Z_over_A_LnI, radlen);
  mLnIGas=rho_Z_over_A_LnI/mRhoZoverAGas;
  mKRhoZoverAGas=0.1535E-3*mRhoZoverAGas;

//  RootGeom->FindMat("FDchamberGas",mRhoZoverAGas,rho_Z_over_A_LnI, radlen);

  // Get the geometry
  geom = dapp->GetDGeometry(0);  // should have run number argument...

  vector<double>sc_origin;
  geom->Get("//posXYZ[@volume='StartCntr']/@X_Y_Z",sc_origin);
  
  vector<double>sc_light_guide;
  geom->Get("//tubs[@name='STLG']/@Rio_Z",sc_light_guide); 
  //sc_light_guide_length=sc_light_guide[2];
  
  vector<vector<double> > sc_rioz;
  geom->GetMultiple("//pgon[@name='STRC']/polyplane/@Rio_Z", sc_rioz);
  
  for (unsigned int k=0;k<sc_rioz.size()-1;k++){
    DVector3 pos((sc_rioz[k][0]+sc_rioz[k][1])/2.,0.,sc_rioz[k][2]+sc_origin[2]);
    DVector3 dir(sc_rioz[k+1][2]-sc_rioz[k][2],0,
		 -sc_rioz[k+1][0]+sc_rioz[k][0]);
    dir.SetMag(1.);
    
    sc_pos.push_back(pos);
    sc_norm.push_back(dir);    
  }
  sc_leg_tcor=(sc_light_guide[2]-sc_pos[0].z())/C_EFFECTIVE;
  double theta=sc_norm[sc_norm.size()-1].Theta();
  sc_angle_cor=1./cos(M_PI-theta); 
}

//---------------------------------
// ~DParticleID    (Destructor)
//---------------------------------
DParticleID::~DParticleID()
{

}

// Group fitted tracks according to candidate id
jerror_t DParticleID::GroupTracks(vector<const DTrackTimeBased *> &tracks,
			      vector<vector<const DTrackTimeBased*> >&grouped_tracks){ 
  if (tracks.size()==0) return RESOURCE_UNAVAILABLE;
 
  JObject::oid_t old_id=tracks[0]->candidateid;
  vector<const DTrackTimeBased *>hypotheses;
  for (unsigned int i=0;i<tracks.size();i++){
    const DTrackTimeBased *track=tracks[i];
    
    if (old_id != track->candidateid){
      // Sort according to FOM
      sort(hypotheses.begin(),hypotheses.end(),DParticleID_hypothesis_cmp);

      // Add to the grouped_tracks vector
      grouped_tracks.push_back(hypotheses);

      // Clear the hypothesis list for the next track
      hypotheses.clear();
    }
    hypotheses.push_back(track);
    old_id=track->candidateid;
  }
  // Final set
  sort(hypotheses.begin(),hypotheses.end(),DParticleID_hypothesis_cmp);
  grouped_tracks.push_back(hypotheses);

  return NOERROR;
}



// Calculate the most probable energy loss per unit length in units of 
// GeV/cm in the FDC or CDC gas for a particle of momentum p and mass "mass"
double DParticleID::GetMostProbabledEdx(double p,double mass,double dx){
  double betagamma=p/mass;
  double beta2=1./(1.+1./betagamma/betagamma);
  if (beta2<1e-6) beta2=1e-6;
  
  // Electron mass 
  double Me=0.000511; //GeV

  // First (non-logarithmic) term in Bethe-Bloch formula
  double mean_dedx=mKRhoZoverAGas/beta2;
 
  // Most probable energy loss from Landau theory (see Leo, pp. 51-52)
  return mean_dedx*(log(mean_dedx*dx)
		    -log((1.-beta2)/2./Me/beta2)-2.*mLnIGas-beta2+0.200);
}

// Empirical form for sigma/mean for gaseous detectors with num_dedx 
// samples and sampling thickness path_length.  Assumes that the number of 
// hits has already been converted from an (unsigned) int to a double.
#define TCUT 100.e-6 // energy cut for bethe-bloch 
double DParticleID::GetdEdxSigma(double num_hits,double p,double mass,
				  double mean_path_length){
  // kinematic quantities
  double betagamma=p/mass;
  double betagamma2=betagamma*betagamma;
  double beta2=1./(1.+1./betagamma2);
  if (beta2<1e-6) beta2=1e-6;

  double Me=0.000511; //GeV
  double m_ratio=Me/mass;
  double two_Me_betagamma_sq=2.*Me*betagamma2;
  double Tmax
    =two_Me_betagamma_sq/(1.+2.*sqrt(1.+betagamma2)*m_ratio+m_ratio*m_ratio);
  // Energy truncation for knock-on electrons
  double T0=(Tmax>TCUT)?TCUT:Tmax;
  
  // Bethe-Bloch
  double mean_dedx=mKRhoZoverAGas/beta2
    *(log(two_Me_betagamma_sq*T0)-2.*mLnIGas-beta2*(1.+T0/Tmax));
  
  return 0.41*mean_dedx*pow(num_hits,-0.43)*pow(mean_path_length,-0.32);
  //return 0.41*mean_dedx*pow(double(num_hits),-0.5)*pow(mean_path_length,-0.32);
}

// Compute the energy losses and the path lengths in the chambers for each hit 
// on the track. Returns a list of dE and dx pairs with the momentum at the 
// hit.
jerror_t DParticleID::GetdEdx(const DTrackTimeBased *track,
			       vector<dedx_t>&dEdx_list){
  // Position and momentum
  DVector3 pos,mom;
  
  //dE and dx pairs
  pair<double,double>de_and_dx;

  // We cast away the const-ness of the reference trajectory so that we can use the DisToRT method
  DReferenceTrajectory *my_rt=const_cast<DReferenceTrajectory*>(track->rt);

  //Get the list of cdc hits used in the fit
  vector<const DCDCTrackHit*>cdchits;
  track->GetT(cdchits);

  // Loop over cdc hits
  for (unsigned int i=0;i<cdchits.size();i++){
    double locReturnValue = my_rt->DistToRT(cdchits[i]->wire);
    if(!((locReturnValue >= 0.0) || (locReturnValue <= 0.0)))
      continue; //NaN

    my_rt->GetLastDOCAPoint(pos, mom);

    // Create the dE,dx pair from the position and momentum using a helical approximation for the path 
    // in the straw and keep track of the momentum in the active region of the detector
    if (CalcdEdxHit(mom,pos,cdchits[i],de_and_dx)==NOERROR){
      dEdx_list.push_back(dedx_t(de_and_dx.first,de_and_dx.second,
				 mom.Mag()));
    }
  }
  
  //Get the list of fdc hits used in the fit
  vector<const DFDCPseudo*>fdchits;
  track->GetT(fdchits);

  // loop over fdc hits
  for (unsigned int i=0;i<fdchits.size();i++){
    double locReturnValue = my_rt->DistToRT(fdchits[i]->wire);
    if(!((locReturnValue >= 0.0) || (locReturnValue <= 0.0)))
      continue; //NaN
    my_rt->GetLastDOCAPoint(pos, mom);
   
    double gas_thickness=1.0; // cm
    dEdx_list.push_back(dedx_t(fdchits[i]->dE,
			       gas_thickness/cos(mom.Theta()),
			       mom.Mag()));
  }
    
  // Sort the dEdx entries from smallest to largest
  sort(dEdx_list.begin(),dEdx_list.end(),DParticleID_dedx_cmp);  
 
  return NOERROR;
}





// Calculate the path length for a single hit in a straw and return ds and the 
// energy deposition in the straw.  It returns dE as the first element in the 
// dedx pair and ds as the second element in the dedx pair.
jerror_t DParticleID::CalcdEdxHit(const DVector3 &mom,
				  const DVector3 &pos,
				  const DCDCTrackHit *hit,
				  pair <double,double> &dedx){
  if (hit==NULL || hit->wire==NULL) return RESOURCE_UNAVAILABLE;
  
  // Track direction parameters
  double phi=mom.Phi();
  double lambda=M_PI_2-mom.Theta();
  double cosphi=cos(phi);
  double sinphi=sin(phi);
  double tanl=tan(lambda);
  
  //Position relative to wire origin
  double dz=pos.z()-hit->wire->origin.z();
  double dx=pos.x()-hit->wire->origin.x();
  double dy=pos.y()-hit->wire->origin.y();
  
  // square of straw radius
  double rs2=0.776*0.776;
  
  // Useful temporary variables related to the direction of the wire
  double ux=hit->wire->udir.x();
  double uy=hit->wire->udir.y();
  double uz=hit->wire->udir.z();
  double A=1.-ux*ux;
  double B=-2.*ux*uy;
  double C=-2.*ux*uz;
  double D=-2.*uy*uz;
  double E=1.-uy*uy;
  double F=1.-uz*uz;  

  // The path length in the straw is given by  s=sqrt(b*b-4*a*c)/a/cosl.
  // a, b, and c follow.
  double a=A*cosphi*cosphi+B*cosphi*sinphi+C*cosphi*tanl+D*sinphi*tanl
    +E*sinphi*sinphi+F*tanl*tanl;
  double b=2.*A*dx*cosphi+B*dx*sinphi+B*dy*cosphi+C*dx*tanl+C*cosphi*dz
    +D*dy*tanl+D*sinphi*dz+2.*E*dy*sinphi+2.*F*dz*tanl;
  double c=A*dx*dx+B*dx*dy+C*dx*dz+D*dy*dz+E*dy*dy+F*dz*dz-rs2;
  
  // Check for valid arc length and compute dEdx
  double temp=b*b-4.*a*c;
  if (temp>0){
    double cosl=fabs(cos(lambda));
    //    double gas_density=0.0018;

    // arc length and energy deposition
    //dedx.second=gas_density*sqrt(temp)/a/cosl; // g/cm^2
    dedx.second=sqrt(temp)/a/cosl;
    dedx.first=hit->dE; //GeV

    return NOERROR;
  }
  
  return VALUE_OUT_OF_RANGE;
}
  
//------------------
// MatchToTOF
//------------------
// Loop over TOF points, looking for minimum distance of closest approach
// of track to a point in the TOF and using this to check for a match. 
//
// NOTE:  an initial guess for tproj is expected as input so that out-of-time 
// hits can be skipped
jerror_t DParticleID::MatchToTOF(const DReferenceTrajectory *rt, DTrackFitter::fit_type_t fit_type, vector<const DTOFPoint*>&tof_points, double &tproj, unsigned int &tof_match_id, double &locPathLength, double &locFlightTime){
  //tproj=NaN;
  tof_match_id=0;
  if (tof_points.size()==0){
    tproj=NaN;
    return RESOURCE_UNAVAILABLE;
  }

  double dmin=10000.;
  // loop over tof points
  double locTempPathLength;
  for (unsigned int k=0;k<tof_points.size();k++){
    // Check that the hit is not out of time with respect to the track
    if (fabs(tproj-tof_points[k]->t)>OUT_OF_TIME_CUT) continue;

    // Get the TOF cluster position and normal vector for the TOF plane
    DVector3 tof_pos=tof_points[k]->pos;
    DVector3 norm(0,0,1);
    DVector3 proj_pos,dir;
    
    // Find the distance of closest approach between the track trajectory
    // and the tof cluster position, looking for the minimum
    double locTempFlightTime=0.;
    rt->GetIntersectionWithPlane(tof_pos,norm,proj_pos,dir,&locTempPathLength,&locTempFlightTime);
    double d=(tof_pos-proj_pos).Mag();

    if (d<dmin){
      dmin=d;
      locPathLength = locTempPathLength;
      locFlightTime = locTempFlightTime;
      tof_match_id=k;
    }
  }
  
  // Check for a match 
  //  double p=rt->swim_steps[0].mom.Mag();
  double match_cut=0.;
  match_cut = 6.15; //current dPositionMatchCut_DoubleEnded variable in DTOFPoint_factory.cc
//  if (fit_type==DTrackFitter::kTimeBased) match_cut=3.624+0.488/p;
//  else match_cut=4.0+0.488/p;
  if (dmin<match_cut){
    // Projected time at the target
    tproj=tof_points[tof_match_id]->t - locFlightTime;

    return NOERROR;
  }
   
  tproj=NaN;
  return VALUE_OUT_OF_RANGE;
}

//------------------
// MatchToBCAL
//------------------
// Loop over bcal clusters, looking for minimum distance of closest approach
// of track to a cluster and using this to check for a match. 
//
// NOTE:  an initial guess for tproj is expected as input so that out-of-time 
// hits can be skipped
jerror_t DParticleID::MatchToBCAL(const DReferenceTrajectory *rt, DTrackFitter::fit_type_t fit_type, vector<const DBCALShower*>&bcal_showers, double &tproj, unsigned int &bcal_match_id, double &locPathLength, double &locFlightTime){
  //tproj=NaN;
  bcal_match_id=0;
  if (bcal_showers.size()==0){
    tproj=NaN;
    return RESOURCE_UNAVAILABLE;
  }
    
  //Loop over bcal showers
  double dmin=10000.;
  double dphi=1000.,dz=1000.;
  double locTempPathLength;
  for (unsigned int k=0;k<bcal_showers.size();k++){  
    // Check that the hit is not out of time with respect to the track
    if (fabs(tproj-bcal_showers[k]->t)>OUT_OF_TIME_CUT) continue;

    // Get the BCAL cluster position and normal
    const DBCALShower *shower = bcal_showers[k];
    DVector3 bcal_pos(shower->x, shower->y, shower->z); 
    DVector3 proj_pos;
    // and the bcal cluster position, looking for the minimum
    double locTempFlightTime=0.;
    double d = rt->DistToRTwithTime(bcal_pos,&locTempPathLength,&locTempFlightTime);
    proj_pos = rt->GetLastDOCAPoint();
    
    if (d<dmin){
      dmin=d;
      locPathLength = locTempPathLength;
      locFlightTime = locTempFlightTime;
      bcal_match_id=k; 
      dz=proj_pos.z()-bcal_pos.z();
      dphi=proj_pos.Phi()-bcal_pos.Phi();
    }
  }
  
  // Check for a match 
  double p=rt->swim_steps[0].mom.Mag();
  dphi+=0.002+8.314e-3/(p+0.3788)/(p+0.3788);
   double phi_sigma=0.025+5.8e-4/p/p/p;
   if (fabs(dz)<10. && fabs(dphi)<3.*phi_sigma){
     // Projected time at the target
     tproj=bcal_showers[bcal_match_id]->t - locFlightTime;
//     tproj += 0.03; // correction determined from observation of simulated data //Paul Mattione
     return NOERROR;
   }
   
   tproj=NaN;
   return VALUE_OUT_OF_RANGE;
}

//------------------
// MatchToFCAL
//------------------
// Loop over fcal clusters, looking for minimum distance of closest approach
// of track to a cluster and using this to check for a match. 
//
// NOTE:  an initial guess for tproj is expected as input so that out-of-time 
// hits can be skipped
jerror_t DParticleID::MatchToFCAL(const DReferenceTrajectory *rt, DTrackFitter::fit_type_t fit_type, vector<const DFCALShower*>&fcal_showers, double &tproj, unsigned int &fcal_match_id, double &locPathLength, double &locFlightTime){
  //tproj=NaN;
  fcal_match_id=0;
  if (fcal_showers.size()==0){
    tproj=NaN;
    return RESOURCE_UNAVAILABLE;
  }
  
  // Set minimum matching distance to a large default value
  double dmin=10000.;
  // loop over clusters
  double locTempPathLength;
  for (unsigned int k=0;k<fcal_showers.size();k++){
    // Check that the hit is not out of time with respect to the track
    if (fabs(tproj-fcal_showers[k]->getTime())>OUT_OF_TIME_CUT) continue;

    // Get the FCAL cluster position and normal vector for the FCAL plane
    DVector3 fcal_pos=fcal_showers[k]->getPosition();
    // This is a bit of a kludge...
    //fcal_pos.SetZ(DFCALGeometry::fcalFaceZ());
    DVector3 norm(0,0,1);
    DVector3 proj_pos,dir;

    // Find the distance of closest approach between the track trajectory
    // and the tof cluster position, looking for the minimum
    double locTempFlightTime=0.;
    rt->GetIntersectionWithPlane(fcal_pos,norm,proj_pos,dir,&locTempPathLength,&locTempFlightTime);
    double d=(fcal_pos-proj_pos).Mag();

    if (d<dmin){
      dmin=d;
      locPathLength = locTempPathLength;
      locFlightTime = locTempFlightTime;
      fcal_match_id=k;
    }
  }
 
  // Check for a match 
  if(dmin<5.0 /* temporary */){
    tproj=fcal_showers[fcal_match_id]->getTime() - locFlightTime;
    tproj -= 2.218; // correction determined from fit to simulated data 
    return NOERROR;
  }

  tproj=NaN;
  return VALUE_OUT_OF_RANGE;
}

//------------------
// MatchToSC
//------------------
// Match track to the start counter paddles with hits.  If a match
// is found, use the z-position of the track projection to the start counter 
// planes to correct for the light propagation within the scintillator and 
// estimate the "vertex" time.
//
// Unlike the next method, this method does not use the reference trajectory.
//
// NOTE:  an initial guess for tproj is expected as input so that out-of-time 
// hits can be skipped
jerror_t DParticleID::MatchToSC(const DKinematicData &parms, 
				vector<const DSCHit*>&sc_hits, 
				double &tproj,unsigned int &sc_match_id){ 
  sc_match_id=0;
  if (sc_hits.size()==0){
    tproj=NaN;
    return RESOURCE_UNAVAILABLE;
  }
  double myz=0.;
  double dphi_min=10000.,myphi=0.;
  unsigned int num=sc_norm.size()-1;

  DVector3 pos(parms.position());
  DVector3 mom(parms.momentum());
  stepper->SetCharge(parms.charge());

  // Swim to barrel representing the start counter straight portion
  double ds=0.,myds=0;
  if (stepper->SwimToRadius(pos,mom,sc_pos[1].x(),&ds)){
    tproj=NaN;
    return VALUE_OUT_OF_RANGE;
  }
  // Change the sign of ds -- most of the time we will need to add a small 
  // amount of time to the time calculated at the start counter
  ds*=-1.;

  // Position along z and phi at intersection
  double proj_z=pos.z();
  double proj_phi=pos.Phi();
  if (proj_phi<0) proj_phi+=2.*M_PI;

  // Position of transition between start counter nose and leg
  double sc_pos1=sc_pos[1].z();
  // position in z at the end of the nose
  double sc_posn=sc_pos[num].z();

  // loop over sc hits, looking for the one with closest phi value to the 
  // projected phi value
  for (unsigned int i=0;i<sc_hits.size();i++){
    // Check that the hit is not out of time with respect to the track
    if (fabs(tproj-sc_hits[i]->t)>OUT_OF_TIME_CUT) continue;

    double phi=(4.5+9.*(sc_hits[i]->sector-1))*M_PI/180.;
    double dphi=phi-proj_phi;
 
    // If the z position is in the nose region, match to the appropriate start
    // counter plane
    if (proj_z>sc_pos1){
      double cosphi=cos(phi);
      double sinphi=sin(phi);    
      for (unsigned int i=1;i<num;i++){
	DVector3 mymom=(-1.)*mom;
	DVector3 mypos=pos;
	double xhat=sc_norm[i].x(); 
	double r=sc_pos[i].X();
	double x=r*cosphi;
	double y=r*sinphi;
	double z=sc_pos[i].z();
	DVector3 norm(x*xhat,y*xhat,z);  
	DVector3 plane(x,y,z);
	double ds2=0.;
	if (stepper->SwimToPlane(mypos,mymom,plane,norm,&ds2)) continue;

	proj_z=mypos.z();
	if (proj_z<sc_pos[i+1].z()){
	  proj_phi=mypos.Phi();
	  if (proj_phi<0) proj_phi+=2.*M_PI;
	  dphi=proj_phi-phi;
	  ds+=ds2;

	  break;
	}
      }
    }

    // Look for smallest difference in phi
    if (fabs(dphi)<dphi_min){
      dphi_min=fabs(dphi);
      myphi=phi;
      myz=proj_z;
      myds=ds;
      sc_match_id=i;
    }
  }
  
  // Look for a match in phi
  if (dphi_min<0.16){
    // Find the time at the start counter, before correcting for position
    // along the start counter
    tproj=sc_hits[sc_match_id]->t-sc_leg_tcor;
    
    // Check position along z
    if (myz<sc_pos[0].z()) myz=sc_pos[0].z();
    if (myz<sc_pos1){
      // Leg region
      tproj-=myz/C_EFFECTIVE;
    }
    else if (myz<sc_posn){
      // Nose region
      tproj-=((myz-sc_pos1)*sc_angle_cor+sc_pos1)/C_EFFECTIVE;
    }
    else{
      // Assume that the particle hit the most downstream z position of the
      // start counter
      tproj-=((sc_posn-sc_pos1)*sc_angle_cor+sc_pos1)/C_EFFECTIVE;
    }
	   
    // Adjust for flight time to the position pos
    double mass=parms.mass();
    double p2=mom.Mag2();
    double one_over_beta=sqrt(1.+mass*mass/p2);   
    tproj += myds*one_over_beta/SPEED_OF_LIGHT;

    return NOERROR;
  }

  tproj=NaN;
  return VALUE_OUT_OF_RANGE;

}


//------------------
// MatchToSC
//------------------
// Match track to the start counter paddles with hits.  If a match
// is found, use the z-position of the track projection to the start counter 
// planes to correct for the light propagation within the scintillator and 
// estimate the "vertex" time.
//
// NOTE:  an initial guess for tproj is expected as input so that out-of-time 
// hits can be skipped
jerror_t DParticleID::MatchToSC(const DReferenceTrajectory *rt, DTrackFitter::fit_type_t fit_type, vector<const DSCHit*>&sc_hits, double &tproj,unsigned int &sc_match_id, double &locPathLength, double &locFlightTime){

  //tproj=NaN;
	sc_match_id=0;
	if (sc_hits.size()==0){
	  tproj=NaN;
	  return RESOURCE_UNAVAILABLE;
	}
	double myz=0.;
	double dphi_min=10000.,myphi=0.;
	double locTempPathLength, locTempFlightTime = 0.0;
	DVector3 proj_pos;

	// Find intersection with a "barrel" approximation for the start counter
	rt->GetIntersectionWithRadius(sc_pos[1].x(),proj_pos,&locTempPathLength,&locTempFlightTime);
	double proj_phi=proj_pos.Phi();
	if (proj_phi<0) proj_phi+=2.*M_PI;

	// loop over sc hits, looking for the one with closest phi value to the 
	// projected phi value
	for (unsigned int i=0;i<sc_hits.size();i++){
	  // Check that the hit is not out of time with respect to the track
	  if (fabs(tproj-sc_hits[i]->t)>OUT_OF_TIME_CUT) continue;

	  double phi=(4.5+9.*(sc_hits[i]->sector-1))*M_PI/180.;
	  double dphi=phi-proj_phi;

	  if (fabs(dphi)<dphi_min){
	    dphi_min=fabs(dphi);
	    myphi=phi;
	    myz=proj_pos.z();
	    sc_match_id=i;
	  }
	}
	// Look for a match in phi
	if (dphi_min<0.16){
		// Now check to see if the intersection is in the nose region and find the
		// start time
		tproj=sc_hits[sc_match_id]->t-sc_leg_tcor;
		if (myz<sc_pos[0].z()) myz=sc_pos[0].z();
		if (myz>sc_pos[1].z()){
			unsigned int num=sc_norm.size()-1;
			for (unsigned int i=1;i<num;i++){
				double xhat=sc_norm[i].x();
				DVector3 norm(cos(myphi)*xhat,sin(myphi)*xhat,sc_norm[i].z());
				double r=sc_pos[i].X();
				DVector3 pos(r*cos(myphi),r*sin(myphi),sc_pos[i].z());
				rt->GetIntersectionWithPlane(pos,norm,proj_pos,&locTempPathLength,&locTempFlightTime);
				myz=proj_pos.z();
				if (myz<sc_pos[i+1].z())
					break;
			}
			double sc_pos1=sc_pos[1].z();
			if (myz<sc_pos1){
				tproj-=locTempFlightTime+sc_pos1/C_EFFECTIVE;
				locFlightTime = locTempFlightTime;
				locPathLength = locTempPathLength;
			}else if (myz>sc_pos[num].z()){
				// Assume that the particle hit the most downstream z position of the
				// start counter
				double costheta=rt->swim_steps[0].mom.CosTheta();
				double s=(sc_pos[num].z()-rt->swim_steps[0].origin.z())/costheta;
				double mass=rt->GetMass();
				double p2=rt->swim_steps[0].mom.Mag2();
				double one_over_beta=sqrt(1.+mass*mass/p2);
	
				tproj -= s*one_over_beta/SPEED_OF_LIGHT + ((sc_pos[num].z()-sc_pos1)*sc_angle_cor+sc_pos1)/C_EFFECTIVE;
				locFlightTime = s*one_over_beta/SPEED_OF_LIGHT;
				locPathLength = s;
			}else{
				tproj-=locTempFlightTime+((myz-sc_pos1)*sc_angle_cor+sc_pos1)/C_EFFECTIVE;
				locFlightTime = locTempFlightTime;
				locPathLength = locTempPathLength;
			}
		}else{
			tproj-=locTempFlightTime+myz/C_EFFECTIVE;
			locFlightTime = locTempFlightTime;
			locPathLength = locTempPathLength;
		}
		return NOERROR;
	}

	tproj=NaN;
	return VALUE_OUT_OF_RANGE;
}

Particle_t DParticleID::IDTrack(float locCharge, float locMass){
	float locMassTolerance = 0.010;
	if (locCharge > 0.0){ // Positive particles
		if (fabs(locMass - ParticleMass(Proton)) < locMassTolerance) return Proton;
		if (fabs(locMass - ParticleMass(PiPlus)) < locMassTolerance) return PiPlus;
		if (fabs(locMass - ParticleMass(KPlus)) < locMassTolerance) return KPlus;
		if (fabs(locMass - ParticleMass(Positron)) < locMassTolerance) return Positron;
		if (fabs(locMass - ParticleMass(MuonPlus)) < locMassTolerance) return MuonPlus;
//		if (locMass < 0.3) return PiPlus;
//		else if (locMass >= 0.3 && locMass < 0.7) return KPlus;
//		else if (locMass >= 0.7 && locMass < 1.0) return Proton;
	} else if (locCharge < 0.0){ // Negative particles
		if (fabs(locMass - ParticleMass(PiMinus)) < locMassTolerance) return PiMinus;
		if (fabs(locMass - ParticleMass(KMinus)) < locMassTolerance) return KMinus;
		if (fabs(locMass - ParticleMass(MuonMinus)) < locMassTolerance) return MuonMinus;
		if (fabs(locMass - ParticleMass(Electron)) < locMassTolerance) return Electron;
		if (fabs(locMass - ParticleMass(AntiProton)) < locMassTolerance) return AntiProton;
//		if (locMass < 0.3) return PiMinus;
//		else if (locMass >= 0.3 && locMass < 0.7) return KMinus;
	} else { //Neutral Track
		if (fabs(locMass - ParticleMass(Gamma)) < locMassTolerance) return Gamma;
		if (fabs(locMass - ParticleMass(Neutron)) < locMassTolerance) return Neutron;
//		return ((locMass > 0.4) ? Neutron : Gamma);
	}
	return Unknown;
}

