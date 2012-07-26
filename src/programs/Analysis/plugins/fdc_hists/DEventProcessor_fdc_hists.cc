// $Id: DEventProcessor_fdc_hists.cc 2774 2007-07-19 15:59:02Z davidl $
//
//    File: DEventProcessor_fdc_hists.cc
// Created: Sun Apr 24 06:45:21 EDT 2005
// Creator: davidl (on Darwin Harriet.local 7.8.0 powerpc)
//

#include <iostream>
#include <cmath>
using namespace std;

#include <TThread.h>
#include <TMath.h>
#include <TROOT.h>

#include "DEventProcessor_fdc_hists.h"

#include <JANA/JApplication.h>
#include <JANA/JEventLoop.h>
#include <JANA/JCalibration.h>

#include <DANA/DApplication.h>
#include <TRACKING/DMCThrown.h>
#include <TRACKING/DMCTrackHit.h>
#include <FDC/DFDCGeometry.h>
#include <FDC/DFDCHit.h>
#include <FDC/DFDCPseudo.h>
#include <FDC/DFDCSegment.h>
#include <FDC/DFDCCathodeCluster.h>
#include <FCAL/DFCALShower.h>
#include <BCAL/DBCALShower.h>
#include <DVector2.h>

#define EPS 1e-3
#define ITER_MAX 10
#define ADJACENT_MATCH_RADIUS 1.0
#define MATCH_RADIUS 2.0

// The executable should define the ROOTfile global variable. It will
// be automatically linked when dlopen is called.
//extern TFile *ROOTfile;

// Routine used to create our DEventProcessor
extern "C"{
void InitPlugin(JApplication *app){
	InitJANAPlugin(app);
	app->AddProcessor(new DEventProcessor_fdc_hists());
}
} // "C"


//------------------
// DEventProcessor_fdc_hists
//------------------
DEventProcessor_fdc_hists::DEventProcessor_fdc_hists()
{
	fdc_ptr = &fdc;
	fdchit_ptr = &fdchit;

	pthread_mutex_init(&mutex, NULL);
}

//------------------
// ~DEventProcessor_fdc_hists
//------------------
DEventProcessor_fdc_hists::~DEventProcessor_fdc_hists()
{
}

//------------------
// init
//------------------
jerror_t DEventProcessor_fdc_hists::init(void)
{
  cout << "initializing" <<endl;

	mT0=0.;

	alignments.resize(24);
	
	for (unsigned int i=0;i<24;i++){
	  alignments[i].A(kDx)=alignments[i].A(kDy)=alignments[i].A(kDPhi)=0.;
	  alignments[i].E(kDx,kDx)=alignments[i].E(kDy,kDy)=1.0;
	  alignments[i].E(kDPhi,kDPhi)=1.0;
	}
	

	TDirectory *dir = new TDirectoryFile("FDC","FDC");
	dir->cd();

	// Create Tree
	fdctree = new TTree("fdc","FDC algnments");
	fdcbranch = fdctree->Branch("T","FDC_branch",&fdc_ptr);

	// Go back up to the parent directory
	dir->cd("../");
	
	return NOERROR;
}

//------------------
// brun
//------------------
jerror_t DEventProcessor_fdc_hists::brun(JEventLoop *loop, int runnumber)
{	
  DApplication* dapp=dynamic_cast<DApplication*>(loop->GetJApplication());
  const DGeometry *dgeom  = dapp->GetDGeometry(runnumber);
  dgeom->GetFDCWires(fdcwires);

  // Get the position of the CDC downstream endplate from DGeometry
  double endplate_dz,endplate_rmin,endplate_rmax;
  dgeom->GetCDCEndplate(endplate_z,endplate_dz,endplate_rmin,endplate_rmax);
  endplate_z+=0.5*endplate_dz;

  dapp->Lock();
    

  Hqratio_vs_wire= (TH2F *)gROOT->FindObject("Hqratio_vs_wire");
  if (!Hqratio_vs_wire)
    Hqratio_vs_wire= new TH2F("Hqratio_vs_wire","Charge ratio vs wire number",
			      2304,0.5,2304.5,100,-0.5,0.5);

  Hdelta_z_vs_wire= (TH2F*)gROOT->FindObject("Hdelta_z_vs_wire");
  if (!Hdelta_z_vs_wire)
    Hdelta_z_vs_wire= new TH2F("Hdelta_z_vs_wire","wire z offset vs wire number",
			      2304,0.5,2304.5,100,-0.1,0.1);

    Hwire_prob = (TH1F*)gROOT->FindObject("Hwire_prob");
    if (!Hwire_prob) 
      Hwire_prob=new TH1F("Hwire_prob","Confidence level for wire-based fit",
			  101,-0.005,1.005); 

    Hreduced_chi2 = (TH1F*)gROOT->FindObject("Hreduced_chi2");
    if (!Hreduced_chi2) 
      Hreduced_chi2=new TH1F("Hreduced_chi2","chi^2/ndf",
			  1000,0,100); 
    Htime_prob = (TH1F*)gROOT->FindObject("Htime_prob");
    if (!Htime_prob) 
      Htime_prob=new TH1F("Htime_prob","Confidence level for time-based fit",
			  100,0,1);

    Hwire_res_vs_wire=(TH2F*)gROOT->FindObject("Hwire_res_vs_wire");
    if (!Hwire_res_vs_wire){
      Hwire_res_vs_wire=new TH2F("Hwire_res_vs_wire","wire-based residuals",
				  2304,0.5,2304.5,100,-1,1);
    }  
    Hvres_vs_wire=(TH2F*)gROOT->FindObject("Hvres_vs_wire");
    if (!Hvres_vs_wire){
      Hvres_vs_wire=new TH2F("Hvres_vs_wire","residual for position along wire",
				  2304,0.5,2304.5,200,-0.5,0.5);
    } 
    Htime_res_vs_wire=(TH2F*)gROOT->FindObject("Htime_res_vs_wire");
    if (!Htime_res_vs_wire){
      Htime_res_vs_wire=new TH2F("Htime_res_vs_wire","time-based residuals",
				  2304,0.5,2304.5,50,-0.1,0.1);
    }
    Hcand_ty_vs_tx=(TH2F*)gROOT->FindObject("Hcand_ty_vs_tx");
    if (!Hcand_ty_vs_tx){
      Hcand_ty_vs_tx=new TH2F("Hcand_ty_vs_tx","candidate ty vs tx",100,-1,1,
			      100,-1,1);
    }    
    Hwire_ty_vs_tx=(TH2F*)gROOT->FindObject("Hwire_ty_vs_tx");
    if (!Hwire_ty_vs_tx){
      Hwire_ty_vs_tx=new TH2F("Hwire_ty_vs_tx","wire-based ty vs tx",100,-1,1,
			      100,-1,1);
    }  
    Htime_ty_vs_tx=(TH2F*)gROOT->FindObject("Htime_ty_vs_tx");
    if (!Htime_ty_vs_tx){
      Htime_ty_vs_tx=new TH2F("Htime_ty_vs_tx","time-based ty vs tx",100,-1,1,
			      100,-1,1);
    }
    Htime_y_vs_x=(TH3F*)gROOT->FindObject("Htime_y_vs_x");
    if (!Htime_y_vs_x){
      Htime_y_vs_x=new TH3F("Htime_y_vs_x","time-based y vs x",24,0.5,24.5,100,-48.5,48.5,
			    100,-48.5,48.5);
    }
    
    Hdrift_time=(TH1F*)gROOT->FindObject("Hdrift_time");
    if (!Hdrift_time){
      Hdrift_time=new TH1F("Hdrift_time",
			   "drift time",200,-20,380);
    }  
    Hdrift_integral=(TH1F*)gROOT->FindObject("Hdrift_integral");
    if (!Hdrift_integral){
      Hdrift_integral=new TH1F("Hdrift_integral",
			   "drift time integral",140,-20,260);
    }

    Hres_vs_drift_time=(TH2F*)gROOT->FindObject("Hres_vs_drift_time");
    if (!Hres_vs_drift_time){
      Hres_vs_drift_time=new TH2F("Hres_vs_drift_time","Residual vs drift time",320,-20,300,100,-1,1);
    } 
    Hdv_vs_dE=(TH2F*)gROOT->FindObject("Hdv_vs_dE");
    if (!Hdv_vs_dE){
      Hdv_vs_dE=new TH2F("Hdv_vs_dE","dv vs energy dep",100,0,20e-6,100,-1,1);
    }

    Hxcand_prob = (TH1F *)gROOT->FindObject("Hscand_prob");
    if (!Hxcand_prob){
      Hxcand_prob=new TH1F("Hxcand_prob","x candidate prob",100,0,1);
    }  
    Hycand_prob = (TH1F *)gROOT->FindObject("Hycand_prob");
    if (!Hycand_prob){
      Hycand_prob=new TH1F("Hycand_prob","y candidate prob",100,0,1);
    } 
  
  dapp->Unlock();

  JCalibration *jcalib = dapp->GetJCalibration(0);  // need run number here
  vector< map<string, float> > tvals;
  if (jcalib->Get("FDC/fdc_drift_test", tvals)==false){
    for(unsigned int i=0; i<tvals.size(); i++){
      map<string, float> &row = tvals[i];
      fdc_drift_table[i]=row["0"];
      Hdrift_integral->Fill(2.*i-20,fdc_drift_table[i]/0.5);
    }
  }
  else{
    jerr << " FDC time-to-distance table not available... bailing..." << endl;
    exit(0);
  }



  return NOERROR;
}

//------------------
// erun
//------------------
jerror_t DEventProcessor_fdc_hists::erun(void)
{
 

	return NOERROR;
}

//------------------
// fini
//------------------
jerror_t DEventProcessor_fdc_hists::fini(void)
{

	return NOERROR;
}

//------------------
// evnt
//------------------
jerror_t DEventProcessor_fdc_hists::evnt(JEventLoop *loop, int eventnumber)
{
  double NEVENTS=1e6;
  double anneal_factor=pow(10000.0,(NEVENTS-double(eventnumber))/(NEVENTS-1.));
  anneal_factor=1.;
  myevt=eventnumber;

  vector<const DFCALShower*>fcalshowers;
  loop->Get(fcalshowers);
  vector<const DBCALShower*>bcalshowers;
  loop->Get(bcalshowers);
 
  double target_z=65.;

  double outer_z=0.,outer_dz=0.,slope_x=0.,slope_y=0.;
  double outer_time=0.;
  bool got_outer=false;
  if (fcalshowers.size()==1){
    got_outer=true;

    outer_time=fcalshowers[0]->getTime();
    outer_z=fcalshowers[0]->getPosition().z();
    outer_dz=outer_z-target_z;
    
    slope_x=fcalshowers[0]->getPosition().x()/outer_dz;
    slope_y=fcalshowers[0]->getPosition().y()/outer_dz;

    double tanl=1./sqrt(slope_x*slope_x+slope_y*slope_y);
  
    mT0=outer_time-(outer_z-endplate_z)/(29.98*sin(atan(tanl)));
    mT0-=2.218; // ns, empirical correction
  }
  else if (bcalshowers.size()==1){
    got_outer=true;

    outer_time=bcalshowers[0]->t;
    outer_z=bcalshowers[0]->z;
    outer_dz=outer_z-target_z;

    slope_x=bcalshowers[0]->x/outer_dz;
    slope_y=bcalshowers[0]->y/outer_dz;

    double tanl=1./sqrt(slope_x*slope_x+slope_y*slope_y);

    mT0=outer_time-(outer_z-endplate_z)/(29.98*sin(atan(tanl)));
  }

  if (got_outer){
    vector<const DFDCPseudo*>pseudos;
    loop->Get(pseudos);

    vector<const DFDCPseudo*>packages[4];

    for (unsigned int i=0;i<pseudos.size();i++){
      vector<const DFDCCathodeCluster*>cathode_clusters;
      pseudos[i]->GetT(cathode_clusters);

      unsigned int wire_number
	=96*(pseudos[i]->wire->layer-1)+pseudos[i]->wire->wire;
      double q1=0,q2=0;
      for (unsigned int j=0;j<cathode_clusters[0]->members.size();j++){
	double q=cathode_clusters[0]->members[j]->q;
	if (q>q1) q1=q;
      }   
      for (unsigned int j=0;j<cathode_clusters[1]->members.size();j++){
	double q=cathode_clusters[1]->members[j]->q;
	if (q>q2) q2=q;
      }

      double ratio=q2/q1;
      Hqratio_vs_wire->Fill(wire_number,ratio-1.);
      Hdelta_z_vs_wire->Fill(wire_number,0.5336*(1.-ratio)/(1.+ratio));



      packages[(pseudos[i]->wire->layer-1)/6].push_back(pseudos[i]);

    }
    
    // Link hits in each package together into track segments
    vector<segment_t>segments[4];
    for (unsigned int i=0;i<4;i++){  
      FindSegments(packages[i],segments[i]);
    }
    /*
    for (unsigned int k=0;k<4;k++){
      for (unsigned int i=0;i<segments[k].size();i++){
	printf("pack %d seg %d\n",k,i);

	if (segments[k][i].hits.size()>=5)
	  DoFilter(anneal_factor,segments[k][i].hits);
      }
    }
    */
    // Link the segments together to form track candidadates
    vector<vector<const DFDCPseudo *> >LinkedSegments;
    LinkSegments(segments,LinkedSegments);
  
    // Loop over the list of linked segments and perform a kalman filter to find the offsets and 
    // rotations for each wire plane
    jerror_t error=NOERROR;
    for (unsigned int k=0;k<LinkedSegments.size();k++){
      if (LinkedSegments[k].size()>=24)
	error=DoFilter(anneal_factor,LinkedSegments[k]);
      //    printf("error %d\n",error);
    }
    
  }
  
  return NOERROR;
}

// Steering routine for the kalman filter
jerror_t DEventProcessor_fdc_hists::DoFilter(double anneal_factor,
					     vector<const DFDCPseudo *>&hits){
  unsigned int num_hits=hits.size();
  vector<update_t>updates(num_hits);
  vector<update_t>smoothed_updates(num_hits);
  
  // Fit the points to find an initial guess for the line
  double chi2x,chi2y;
  int myndf=num_hits-2; 
  DMatrix4x1 S=FitLine(hits,chi2x,chi2y);     
  double probx=TMath::Prob(chi2x,myndf);
  double proby=TMath::Prob(chi2y,myndf);
 
  Hxcand_prob->Fill(probx);
  Hycand_prob->Fill(proby);

  // Minimum angle
  double tanl=1./sqrt(S(state_tx)*S(state_tx)+S(state_ty)*S(state_ty));
  double theta=90-180/M_PI*atan(tanl);
 
  if (probx>0.01 && proby>0.01 && theta>1.){
    Hcand_ty_vs_tx->Fill(S(state_tx),S(state_ty));  

    // Best guess for state vector at "vertex"
    DMatrix4x1 Sbest;
      
    // Use the result from the initial line fit to form a reference trajectory 
    // for the track. 
    deque<trajectory_t>trajectory;
    // double start_z=hits[0]->wire->origin.z()-1.;
    S(state_x)+=endplate_z*S(state_tx);
    S(state_y)+=endplate_z*S(state_ty);
    SetReferenceTrajectory(endplate_z,S,trajectory,hits);
 
    // Intial guess for covariance matrix
    DMatrix4x4 C,C0,Cbest;
    C0(state_x,state_x)=C0(state_y,state_y)=1.;
    C0(state_tx,state_tx)=C0(state_ty,state_ty)=0.01;

    // Chi-squared and degrees of freedom
    double chi2=1e16,chi2_old=1e16;
    unsigned int ndof=0,ndof_old=0;
    unsigned iter=0;
    for(;;){
      iter++;
      chi2_old=chi2; 
      ndof_old=ndof;
      C=C0;
      if (KalmanFilter(anneal_factor,S,C,hits,trajectory,updates,chi2,ndof)
	  !=NOERROR) break;
      
      //printf("=======chi2 %f\nb",chi2);
      if (chi2>chi2_old || fabs(chi2_old-chi2)<0.1 || iter==ITER_MAX) break;  
      
      // Save the current state and covariance matrixes
      Cbest=C;
      Sbest=S;
      
      // run the smoother (opposite direction to filter)
      Smooth(S,C,trajectory,updates,smoothed_updates);
    }

    if (iter>1){
      double reduced_chi2=chi2_old/double(ndof_old);
      double prob=TMath::Prob(chi2_old,ndof_old);
      Hwire_prob->Fill(prob);
      Hreduced_chi2->Fill(reduced_chi2);
      
      if (reduced_chi2<10){       
	Hwire_ty_vs_tx->Fill(Sbest(state_tx),Sbest(state_ty));
	
	DMatrix2x3 G;// matrix relating alignment vector to measurement coords.
	DMatrix3x2 G_T; // .. and its transpose

	for (unsigned int i=0;i<num_hits;i++){
	  if (smoothed_updates[i].id>0){
	    double x=smoothed_updates[i].S(state_x);
	    double y=smoothed_updates[i].S(state_y);
	    double cosa=hits[i]->wire->udir.y();
	    double sina=hits[i]->wire->udir.x();
	    double u=hits[i]->w;
	    double v=hits[i]->s;
	    double upred=x*cosa-y*sina;
	    double vpred=y*cosa+x*sina;
	    
	    double tu=smoothed_updates[i].S(state_tx)*cosa
	      -smoothed_updates[i].S(state_ty)*sina;
	    double alpha=atan(tu);
	    double cosalpha=cos(alpha);
	    
	    // Get the aligment vector and error matrix for this layer
	    unsigned int layer=hits[i]->wire->layer-1;
	    DMatrix3x3 E=alignments[layer].E;
	    DMatrix3x1 A=alignments[layer].A;
	    double dx=A(kDx);
	    double dy=A(kDy);
	    double sindphi=sin(A(kDPhi));
	    double cosdphi=cos(A(kDPhi));
	    
	    // Transform from alignment vector coords to measurement coords
	    G(0,kDx)=G_T(kDx,0)=-cosa*cosalpha;
	    G(0,kDy)=G_T(kDy,0)=+sina*cosalpha;
	    G(1,kDx)=G_T(kDx,1)=-sina;
	    G(1,kDy)=G_T(kDy,1)=-cosa;
	    G(0,kDPhi)=G_T(kDPhi,0)=(-sindphi*upred+cosdphi*vpred)*cosalpha; 
	    G(1,kDPhi)=G_T(kDPhi,1)=-sindphi*vpred-cosdphi*upred;
	    
	    DMatrix2x2 InvV=(smoothed_updates[i].R+G*E*G_T).Invert();
	    	  
	    // Difference between measurement and projection
	    DMatrix2x1 Mdiff;
	    Mdiff(0)=
	      (u-(upred*cosdphi+vpred*sindphi-dx*cosa+dy*sina))*cosalpha
	      +smoothed_updates[i].drift;
	    Mdiff(1)=v-(vpred*cosdphi-upred*sindphi-dx*sina-dy*cosa);
	    
	    // update the alignment vector and covariance
	    DMatrix3x2 Ka=(E*G_T)*InvV;
	    DMatrix3x1 dA=Ka*Mdiff;
	    DMatrix3x3 Etemp=E-Ka*G*E;
	    if (Etemp(0,0)>0 && Etemp(1,1)>0 && Etemp(2,2)>0){
	      alignments[layer].E=Etemp;
	      alignments[layer].A=A+Ka*Mdiff;	  
	      
	      double dxr=alignments[layer].A(kDx);
	      double dyr=alignments[layer].A(kDy);  
	      fdc.dPhi=alignments[layer].A(kDPhi);
	      double cosdphi=cos(fdc.dPhi);
	      double sindphi=sin(fdc.dPhi);
	      double dx=dxr*cosdphi+dyr*sindphi;
	      double dy=-dxr*sindphi+dyr*cosdphi;
	      fdc.dX=dx;
	      fdc.dY=dy;
	      fdc.layer=layer;
	      fdc.N=myevt;
	      
	      // Lock mutex
	      pthread_mutex_lock(&mutex);
	      
	      fdctree->Fill();
	      
	      // Unlock mutex
	      pthread_mutex_unlock(&mutex);
	    }
	    
	    int wire_id=96*(hits[i]->wire->layer-1)+hits[i]->wire->wire;
	    Hwire_res_vs_wire->Fill(wire_id,Mdiff(0));
	    Hvres_vs_wire->Fill(wire_id,Mdiff(1));
	    Hres_vs_drift_time->Fill(smoothed_updates[i].drift_time,Mdiff(0));
	    Hdv_vs_dE->Fill(hits[i]->dE,Mdiff(1));
	  }
	}
      }
    }

    /*
    printf("-------Event %d\n",myevt);
    for (unsigned int i=0;i<24;i++) 
      printf("A %f %f %f\n",alignments[i].A(kDx),alignments[i].A(kDy),
	     alignments[i].A(kDPhi));
    */

    return NOERROR;
  }

  return VALUE_OUT_OF_RANGE;
}


// Link segments from package to package by doing straight-line projections
jerror_t
DEventProcessor_fdc_hists::LinkSegments(vector<segment_t>segments[4], 
					vector<vector<const DFDCPseudo *> >&LinkedSegments){
  vector<const DFDCPseudo *>myhits;
  for (unsigned int i=0;i<4;i++){
    for (unsigned int j=0;j<segments[i].size();j++){
      if (segments[i][j].matched==false){
	myhits.assign(segments[i][j].hits.begin(),segments[i][j].hits.end());
	
	unsigned i_plus_1=i+1; 
	if (i_plus_1<4){
	  double tx=segments[i][j].S(state_tx);
	  double ty=segments[i][j].S(state_ty);
	  double x0=segments[i][j].S(state_x);
	  double y0=segments[i][j].S(state_y);
	  
	  for (unsigned int k=0;k<segments[i_plus_1].size();k++){
	    if (segments[i_plus_1][k].matched==false){
	      double z=segments[i_plus_1][k].hits[0]->wire->origin.z();
	      DVector2 proj(x0+tx*z,y0+ty*z);
	      
	      if ((proj-segments[i_plus_1][k].hits[0]->xy).Mod()<MATCH_RADIUS){
		segments[i_plus_1][k].matched=true;
		myhits.insert(myhits.end(),segments[i_plus_1][k].hits.begin(),
				segments[i_plus_1][k].hits.end());
		
		unsigned int i_plus_2=i_plus_1+1;
		if (i_plus_2<4){
		  tx=segments[i_plus_1][k].S(state_tx);
		  ty=segments[i_plus_1][k].S(state_ty);
		  x0=segments[i_plus_1][k].S(state_x);
		  y0=segments[i_plus_1][k].S(state_y);
		  
		  for (unsigned int m=0;m<segments[i_plus_2].size();m++){
		    if (segments[i_plus_2][m].matched==false){
		      z=segments[i_plus_2][m].hits[0]->wire->origin.z();
		      proj.Set(x0+tx*z,y0+ty*z);
		      
		      if ((proj-segments[i_plus_2][m].hits[0]->xy).Mod()<MATCH_RADIUS){
			segments[i_plus_2][m].matched=true;
			myhits.insert(myhits.end(),segments[i_plus_2][m].hits.begin(),
				      segments[i_plus_2][m].hits.end());
			
			unsigned int i_plus_3=i_plus_2+1;
			if (i_plus_3<4){
			  tx=segments[i_plus_2][m].S(state_tx);
			  ty=segments[i_plus_2][m].S(state_ty);
			  x0=segments[i_plus_2][m].S(state_x);
			  y0=segments[i_plus_2][m].S(state_y);
			  
			  for (unsigned int n=0;n<segments[i_plus_3].size();n++){
			    if (segments[i_plus_3][n].matched==false){
			      z=segments[i_plus_3][n].hits[0]->wire->origin.z();
			      proj.Set(x0+tx*z,y0+ty*z);
			      
			      if ((proj-segments[i_plus_3][n].hits[0]->xy).Mod()<MATCH_RADIUS){
				segments[i_plus_3][n].matched=true;
				myhits.insert(myhits.end(),segments[i_plus_3][n].hits.begin(),
					      segments[i_plus_3][n].hits.end());
				
				break;
			      } // matched a segment
			    }
			  }  // loop over last set of segments
			} // if we have another package to loop over
			break;
		      } // matched a segment
		    }
		  } // loop over second-to-last set of segments
		}
		break;
	      } // matched a segment
	    }
	  } // loop over third-to-last set of segments
	}	
	LinkedSegments.push_back(myhits);
	myhits.clear();
      } // check if we have already used this segment
    } // loop over first set of segments
  } // loop over packages
  
  return NOERROR;
}

// Find segments by associating adjacent hits within a package together.
jerror_t DEventProcessor_fdc_hists::FindSegments(vector<const DFDCPseudo*>&points,
					vector<segment_t>&segments){
  if (points.size()==0) return RESOURCE_UNAVAILABLE;
  vector<int>used(points.size());

  // Put indices for the first point in each plane before the most downstream
  // plane in the vector x_list.
  double old_z=points[0]->wire->origin.z();
  vector<unsigned int>x_list;
  x_list.push_back(0);
  for (unsigned int i=0;i<points.size();i++){
    used.push_back(false);
    if (points[i]->wire->origin.z()!=old_z){
      x_list.push_back(i);
    }
    old_z=points[i]->wire->origin.z();
  }
  x_list.push_back(points.size()); 

  unsigned int start=0;
  // loop over the start indices, starting with the first plane
  while (start<x_list.size()-1){
    // Now loop over the list of track segment start points
    for (unsigned int i=x_list[start];i<x_list[start+1];i++){
      if (used[i]==false){
	used[i]=true;
	
	// Point in the current plane in the package 
	DVector2 XY=points[i]->xy;
	
	// Create list of nearest neighbors
	vector<const DFDCPseudo*>neighbors;
	neighbors.push_back(points[i]);
	unsigned int match=0;
	double delta,delta_min=1000.;
	for (unsigned int k=0;k<x_list.size()-1;k++){
	  delta_min=1000.;
	  match=0;
	  for (unsigned int m=x_list[k];m<x_list[k+1];m++){
	    delta=(XY-points[m]->xy).Mod();
	    if (delta<delta_min && delta<MATCH_RADIUS){
	      delta_min=delta;
	      match=m;
	    }
	  }	
	  if (match!=0 
	      && used[match]==false
	      ){
	    XY=points[match]->xy;
	    used[match]=true;
	    neighbors.push_back(points[match]);	  
	  }
	}
	unsigned int num_neighbors=neighbors.size();

	bool do_sort=false;
	// Look for hits adjacent to the ones we have in our segment candidate
	for (unsigned int k=0;k<points.size();k++){
	  if (!used[k]){
	    for (unsigned int j=0;j<num_neighbors;j++){
	      delta=(points[k]->xy-neighbors[j]->xy).Mod();

	      if (delta<ADJACENT_MATCH_RADIUS && 
		  abs(neighbors[j]->wire->wire-points[k]->wire->wire)<=1
		  && neighbors[j]->wire->origin.z()==points[k]->wire->origin.z()){
		used[k]=true;
		neighbors.push_back(points[k]);
		do_sort=true;
	      }      
	    }
	  }
	} // loop looking for hits adjacent to hits on segment

	if (neighbors.size()>2){
	  segment_t mysegment;
	  mysegment.matched=false;
	  double chi2x,chi2y;
	  mysegment.S=FitLine(neighbors,chi2x,chi2y);
	  mysegment.hits=neighbors;
	  segments.push_back(mysegment);
	}
      }
    }// loop over start points within a plane
    
    // Look for a new plane to start looking for a segment
    while (start<x_list.size()-1){
      if (used[x_list[start]]==false) break;
      start++;
    }

  }

  return NOERROR;
}

// Use linear regression on the hits to obtain a first guess for the state
// vector.  Method taken from Numerical Recipes in C.
DMatrix4x1 
DEventProcessor_fdc_hists::FitLine(vector<const DFDCPseudo*> &fdchits,
				   double &chi2x,double &chi2y){
  double S1=0.;
  double S1z=0.;
  double S1y=0.;
  double S1zz=0.;
  double S1zy=0.;  
  double S2=0.;
  double S2z=0.;
  double S2x=0.;
  double S2zz=0.;
  double S2zx=0.;

  double sig2v=0.02*0.02; // rough guess;

  for (unsigned int i=0;i<fdchits.size();i++){
    double cosa=fdchits[i]->wire->udir.y();
    double sina=fdchits[i]->wire->udir.x();
    double x=fdchits[i]->xy.X();
    double y=fdchits[i]->xy.Y();
    double z=fdchits[i]->wire->origin.z();
    double sig2x=cosa*cosa/12+sina*sina*sig2v;
    double sig2y=sina*sina/12+cosa*cosa*sig2v;
    double one_over_var1=1/sig2y;
    double one_over_var2=1/sig2x;

    S1+=one_over_var1;
    S1z+=z*one_over_var1;
    S1y+=y*one_over_var1;
    S1zz+=z*z*one_over_var1;
    S1zy+=z*y*one_over_var1;    
    
    S2+=one_over_var2;
    S2z+=z*one_over_var2;
    S2x+=x*one_over_var2;
    S2zz+=z*z*one_over_var2;
    S2zx+=z*x*one_over_var2;
  }
  double D1=S1*S1zz-S1z*S1z;
  double y_intercept=(S1zz*S1y-S1z*S1zy)/D1;
  double y_slope=(S1*S1zy-S1z*S1y)/D1;
  double D2=S2*S2zz-S2z*S2z;
  double x_intercept=(S2zz*S2x-S2z*S2zx)/D2;
  double x_slope=(S2*S2zx-S2z*S2x)/D2;


  // Compute chi2 for the line fits, ignoring correlations between x and y
  chi2x=0;
  chi2y=0;
  for (unsigned int i=0;i<fdchits.size();i++){
    double cosa=fdchits[i]->wire->udir.y();
    double sina=fdchits[i]->wire->udir.x(); 
    double sig2x=cosa*cosa/12+sina*sina*sig2v;
    double sig2y=sina*sina/12+cosa*cosa*sig2v;
    double one_over_var1=1/sig2y;
    double one_over_var2=1/sig2x;

    double z=fdchits[i]->wire->origin.z();
    double dx=fdchits[i]->xy.X()-(x_intercept+x_slope*z);
    double dy=fdchits[i]->xy.Y()-(y_intercept+y_slope*z);

    chi2x+=dx*dx*one_over_var2;
    chi2y+=dy*dy*one_over_var1;
  }
  return DMatrix4x1(x_intercept,y_intercept,x_slope,y_slope);

}

// Kalman smoother 
jerror_t DEventProcessor_fdc_hists::Smooth(DMatrix4x1 &Ss,DMatrix4x4 &Cs,
					   deque<trajectory_t>&trajectory,
					   vector<update_t>updates,
					   vector<update_t>&smoothed_updates
					   ){
  DMatrix4x1 S; 
  DMatrix4x4 C,dC;
  DMatrix4x4 JT,A;

  unsigned int max=trajectory.size()-1;
  S=(trajectory[max].Skk);
  C=(trajectory[max].Ckk);
  JT=(trajectory[max].J.Transpose());
  //Ss=S;
  //Cs=C;
  for (unsigned int m=max-1;m>0;m--){
    if (trajectory[m].h_id==0){
      A=trajectory[m].Ckk*JT*C.Invert();
      Ss=trajectory[m].Skk+A*(Ss-S);
      Cs=trajectory[m].Ckk+A*(Cs-C)*A.Transpose();
    }
    else if (trajectory[m].h_id>0){
      unsigned int id=trajectory[m].h_id-1;
      A=updates[id].C*JT*C.Invert();
      dC=A*(Cs-C)*A.Transpose();
      Ss=updates[id].S+A*(Ss-S);
      Cs=updates[id].C+dC;
      /*
      printf("-------\n");
      updates[id].C;
      Cs.Print();
      */
      smoothed_updates[id].id=trajectory[m].h_id;
      smoothed_updates[id].drift=updates[id].drift;
      smoothed_updates[id].drift_time=updates[id].drift_time;
      smoothed_updates[id].S=Ss;
      smoothed_updates[id].C=Cs;
      smoothed_updates[id].R=updates[id].R-updates[id].H*dC*updates[id].H_T;
    }
    S=trajectory[m].Skk;
    C=trajectory[m].Ckk;
    JT=trajectory[m].J.Transpose();
  }
  /*
    printf("-----end\n");
  Ss.Print();
  Cs.Print();
  printf("--ckk \n");
  C.Print();
  */

  A=trajectory[0].Ckk*JT*C.Invert();
  Ss=trajectory[0].Skk+A*(Ss-S);
  Cs=trajectory[0].Ckk+A*(Cs-C)*A.Transpose();

  return NOERROR;
}

// Perform Kalman Filter for the current trajectory
jerror_t 
DEventProcessor_fdc_hists::KalmanFilter(double anneal_factor,
					DMatrix4x1 &S,DMatrix4x4 &C,
			       vector<const DFDCPseudo *>&hits,
			       deque<trajectory_t>&trajectory,
			       vector<update_t>&updates,
			       double &chi2,unsigned int &ndof){
  DMatrix2x4 H;  // Track projection matrix
  DMatrix4x2 H_T; // Transpose of track projection matrix 
  DMatrix4x2 K;  // Kalman gain matrix
  DMatrix2x2 V(0.0833,0,0,0);  // Measurement covariance 
  DMatrix2x1 Mdiff;
  DMatrix2x2 InvV; // Inverse of error matrix
  DMatrix4x4 I; // identity matrix
  DMatrix4x4 J; // Jacobian matrix
  DMatrix4x1 S0; // State vector from reference trajectory

  //Initialize chi2 and ndof
  chi2=0.;
  ndof=0;

  // Loop over all steps in the trajectory
  S0=trajectory[0].S;
  J=trajectory[0].J;
  trajectory[0].Skk=S;
  trajectory[0].Ckk=C;
  for (unsigned int k=1;k<trajectory.size();k++){
    if (C(0,0)<=0. || C(1,1)<=0. || C(2,2)<=0. || C(3,3)<=0.)
      return UNRECOVERABLE_ERROR;
    
    // Propagate the state and covariance matrix forward in z
    S=trajectory[k].S+J*(S-S0);
    C=J*C*J.Transpose();
    
    // Save the current state and covariance matrix 
    trajectory[k].Skk=S;
    trajectory[k].Ckk=C;

    // Save S and J for the next step
    S0=trajectory[k].S;
    J=trajectory[k].J;

    // Correct S and C for the hit 
    if (trajectory[k].h_id>0){
      unsigned int id=trajectory[k].h_id-1;
      
      double cosa=hits[id]->wire->udir.y();
      double sina=hits[id]->wire->udir.x();
      double x=S(state_x);
      double y=S(state_y);
      double upred=x*cosa-y*sina;
      double vpred=y*cosa+x*sina;
      if (isnan(x) || isnan(y)) return UNRECOVERABLE_ERROR;

      double tu=S(state_tx)*cosa-S(state_ty)*sina;
      double one_plus_tu2=1.+tu*tu;
      double alpha=atan(tu);
      double cosalpha=cos(alpha);
      double sinalpha=sin(alpha);
 
      // Get the aligment vector and error matrix for this layer
      unsigned int layer=hits[id]->wire->layer-1;
      DMatrix3x3 E=alignments[layer].E;
      DMatrix3x1 A=alignments[layer].A;
      double dx=A(kDx);
      double dy=A(kDy);
      double sindphi=sin(A(kDPhi));
      double cosdphi=cos(A(kDPhi));
       
      // Difference between measurement and projection
      for (int m=trajectory[k].num_hits-1;m>=0;m--){
	unsigned int my_id=id+m;
	double u=hits[my_id]->w;
	double v=hits[my_id]->s;
	double du=u-(upred*cosdphi+vpred*sindphi-dx*cosa+dy*sina);
	Mdiff(0)=du*cosalpha;
	Mdiff(1)=v-(vpred*cosdphi-upred*sindphi-dx*sina-dy*cosa);
	
	// Compute drift distance
	double drift_time=hits[my_id]->time-trajectory[k].t;
	double drift=(Mdiff(0)>0?-1.:+1.)*GetDriftDistance(drift_time);
	Mdiff(0)+=drift;
	updates[my_id].drift=drift;
	updates[my_id].drift_time=drift_time;

	// Variance of measurement error
	V(0,0)=anneal_factor*GetDriftVariance(drift_time);
	double sigma=3.0e-8/hits[my_id]->dE+0.007;
	V(1,1)=anneal_factor*sigma*sigma;
	
	// To transform from (x,y) to (u,v), need to do a rotation:
	//   u = x*cosa-y*sina
	//   v = y*cosa+x*sina
	H(0,state_x)=H_T(state_x,0)=(cosa*cosdphi+sina*sindphi)*cosalpha;
	H(0,state_y)=H_T(state_y,0)=(-sina*cosdphi+cosa*sindphi)*cosalpha;
	H(1,state_x)=H_T(state_x,1)=sina*cosdphi-cosa*sindphi;
	H(1,state_y)=H_T(state_y,1)=cosa*cosdphi+sina*sindphi;
	double factor=-du*sinalpha/one_plus_tu2;
	H(0,state_tx)=H_T(state_tx,0)=cosa*factor;
	H(0,state_ty)=H_T(state_ty,0)=-sina*factor;
	
	updates[my_id].H=H;
	updates[my_id].H_T=H_T;
	
	DMatrix2x3 G;
	DMatrix3x2 G_T;

	G(0,kDx)=G_T(kDx,0)=-cosa*cosalpha;
	G(0,kDy)=G_T(kDy,0)=+sina*cosalpha;
	G(1,kDx)=G_T(kDx,1)=-sina;
	G(1,kDy)=G_T(kDy,1)=-cosa;
	G(0,kDPhi)=G_T(kDPhi,0)=(-sindphi*upred+cosdphi*vpred)*cosalpha;	
	G(1,kDPhi)=G_T(kDPhi,1)=-sindphi*vpred-cosdphi*upred;
	
	// Variance for this hit
	InvV=(V+H*C*H_T+G*E*G_T).Invert();
	
	// Compute Kalman gain matrix
	K=(C*H_T)*InvV;
	
	// Update the state vector 
	S+=K*Mdiff;
	updates[my_id].S=S;

	// Update state vector covariance matrix
	C=C-K*(H*C);    
	updates[my_id].C=C;
	
	// Update chi2 for this trajectory
	DMatrix2x1 R=Mdiff-H*K*Mdiff;
	DMatrix2x2 RC=V-H*K*V;
	updates[my_id].R=RC;

	//printf("chi2 %f for %d\n",RC.Chi2(R),my_id);
	
	chi2+=RC.Chi2(R);
	ndof+=2;
      }

    }

  }

  ndof-=4;

  return NOERROR;
}

// Reference trajectory for the track
jerror_t DEventProcessor_fdc_hists
::SetReferenceTrajectory(double z,DMatrix4x1 &S,deque<trajectory_t>&trajectory,
			 vector<const DFDCPseudo *>&pseudos){
  // Jacobian matrix 
  DMatrix4x4 J(1.,0.,1.,0., 0.,1.,0.,1., 0.,0.,1.,0., 0.,0.,0.,1.);
    
  double dz=1.1;
  double t=0.;
  trajectory_t temp;
  temp.S=S;
  temp.J=J;
  temp.Skk=Zero4x1;
  temp.Ckk=Zero4x4;
  temp.h_id=0;	  
  temp.num_hits=0;
  temp.z=z;
  temp.t=0.;
  trajectory.push_front(temp);
  double zhit=z;
  double old_zhit=z;
  unsigned int itrajectory=0;
  for (unsigned int i=0;i<pseudos.size();i++){  
    zhit=pseudos[i]->wire->origin.z();

    if (fabs(zhit-old_zhit)<EPS){
      trajectory_t temp;
      temp.J=J;
      temp.S=trajectory[0].S;
      temp.t=trajectory[0].t;
      temp.h_id=i+1;
      temp.z=trajectory[0].z;
      temp.Skk=Zero4x1;
      temp.Ckk=Zero4x4;
      trajectory.push_front(temp); 

      continue;
    }
    bool done=false;
    while (!done){	    
      double new_z=z+dz;	      
      trajectory_t temp;
      temp.J=J;
      temp.J(state_x,state_tx)=dz;
      temp.J(state_y,state_ty)=dz;
      // Flight time: assume particle is moving at the speed of light
      temp.t=(t+=dz*sqrt(1+S(state_tx)*S(state_tx)+S(state_ty)*S(state_ty))
	      /29.98);
      //propagate the state to the next z position
      temp.S(state_x)=S(state_x)+S(state_tx)*dz;
      temp.S(state_y)=S(state_y)+S(state_ty)*dz;
      temp.S(state_tx)=S(state_tx);
      temp.S(state_ty)=S(state_ty);
      temp.Skk=Zero4x1;
      temp.Ckk=Zero4x4;  
      temp.h_id=0;	  
      temp.num_hits=0;
      if (new_z>zhit){
	new_z=zhit;
	temp.h_id=i+1;
	temp.num_hits=1;
	done=true;
      }
      temp.z=new_z;
      trajectory.push_front(temp); 
      S=temp.S;
      itrajectory++;
      z=new_z;
    }	   
    old_zhit=zhit;
  }
  temp.Skk=Zero4x1;
  temp.Ckk=Zero4x4;
  temp.h_id=0;	  
  temp.z=z+dz;
  temp.J=J;
  temp.J(state_x,state_tx)=-dz;
  temp.J(state_y,state_ty)=-dz;
  // Flight time: assume particle is moving at the speed of light
  temp.t=(t+=dz*sqrt(1+S(state_tx)*S(state_tx)+S(state_ty)*S(state_ty))
	  /29.98);
  //propagate the state to the next z position
  temp.S(state_x)=S(state_x)+S(state_tx)*dz;
  temp.S(state_y)=S(state_y)+S(state_ty)*dz;
  temp.S(state_tx)=S(state_tx);
  temp.S(state_ty)=S(state_ty);
  S=temp.S;
  trajectory.push_front(temp);

  if (false){
    printf("Trajectory:\n");
    for (unsigned int i=0;i<trajectory.size();i++){
    printf(" x %f y %f z %f hit %d\n",trajectory[i].S(state_x),
	   trajectory[i].S(state_y),trajectory[i].z,trajectory[i].h_id); 
    }
  }

  return NOERROR;
}

// Crude approximation for the variance in drift distance due to smearing
double DEventProcessor_fdc_hists::GetDriftVariance(double t){
  if (t<0) t=0;
  double tp=t+1.;
  double tp2=tp*tp;
  double sigma=1.19/tp2+0.0291-1.55e-4*tp+1.68e-6*tp2;
  if (t>100) sigma=0.05;
  return sigma*sigma;
}

#define FDC_T0_OFFSET 20.
// Interpolate on a table to convert time to distance for the fdc
double DEventProcessor_fdc_hists::GetDriftDistance(double t){
  int id=int((t+FDC_T0_OFFSET)/2.);
  if (id<0) id=0;
  if (id>138) id=138;
  double d=fdc_drift_table[id];
  if (id!=138){
    double frac=0.5*(t+FDC_T0_OFFSET-2.*double(id));
    double dd=fdc_drift_table[id+1]-fdc_drift_table[id];
    d+=frac*dd;
  }
  return d;
}
