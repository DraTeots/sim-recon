/*
 *  DBCALCluster_factory.cc
 *
 *  Created by Matthew Shepherd on 3/12/11.
 *
 */

#include <iostream>

using namespace std;

#include "DANA/DApplication.h"
#include "BCAL/DBCALGeometry.h"
#include "BCAL/DBCALHit.h"
#include "BCAL/DBCALUnifiedHit.h"

#include "BCAL/DBCALCluster_factory.h"

#include "units.h"
#include "Math/Minimizer.h"
#include "TMinuitMinimizer.h"
#include <cmath>

bool PointSort( const DBCALPoint* p1, const DBCALPoint* p2 ){

	return ( p1->E() > p2->E() );
}

bool ClusterSort( const DBCALCluster* c1, const DBCALCluster* c2 ){

	return ( c1->E() > c2->E() );
}

DBCALCluster_factory::DBCALCluster_factory() : 
	m_mergeSig( 5 ), 
	m_moliereRadius( 3.7*k_cm ),
	m_clust_hit_timecut ( 20.0*k_nsec ),
	m_timeCut( 8.0*k_nsec ){

	// The phi and theta direction inclusion curves are described in: 
	// http://argus.phys.uregina.ca/gluex/DocDB/0029/002998/003/CAL_meeting_may5.pdf.
	// The theta direction inclusion curve needs to be a function of theta. C1_parm and
	// C2_parm are parameters [0] and [1] in dtheta_inclusion_curve. 
	}

jerror_t
DBCALCluster_factory::init(void){

	return NOERROR;

}

jerror_t
DBCALCluster_factory::fini( void ){

	return NOERROR;
}

jerror_t DBCALCluster_factory::brun(JEventLoop *loop, int32_t runnumber) {
	DApplication* app = dynamic_cast<DApplication*>(loop->GetJApplication());
	DGeometry* geom = app->GetDGeometry(runnumber);
	geom->GetTargetZ(m_z_target_center);

	loop->GetCalib("/BCAL/effective_velocities", effective_velocities);

	loop->GetCalib("/BCAL/attenuation_parameters",attenuation_parameters);

	BCALCLUSTERVERBOSE = 0;
	gPARMS->SetDefaultParameter("BCALCLUSTERVERBOSE", BCALCLUSTERVERBOSE, "VERBOSE level for BCAL Cluster overlap success and conditions");
	//command line parameter to investigate what points are being added to clusters and what clusters are being merged together.

	return NOERROR;
}

jerror_t
DBCALCluster_factory::evnt( JEventLoop *loop, uint64_t eventnumber ){

	vector< const DBCALPoint* > twoEndPoint;
	vector< const DBCALPoint* > usedPoints;
	loop->Get(twoEndPoint);

	// Want to add singled-ended hits to the Clusters. 

	// Looking for hits that are single-ended.

	vector< const DBCALUnifiedHit* > hits;
	loop->Get(hits);

	vector< const DTrackWireBased* > tracks;
	loop->Get(tracks);

	// first arrange the list of hits so they are grouped by cell
	map< int, vector< const DBCALUnifiedHit* > > cellHitMap;
	for( vector< const DBCALUnifiedHit* >::const_iterator hitPtr = hits.begin();
			hitPtr != hits.end();
			++hitPtr ){

		const DBCALUnifiedHit& hit = (**hitPtr);

		int id = DBCALGeometry::cellId( hit.module, hit.layer, hit.sector );

		if( cellHitMap.find( id ) == cellHitMap.end() ){

			cellHitMap[id] = vector< const DBCALUnifiedHit* >();
		}

		cellHitMap[id].push_back( *hitPtr );  
	}

	// now we should try to add on single-ended hits ... 
	vector< const DBCALUnifiedHit* > single_ended_hits; 

	for( map< int, vector< const DBCALUnifiedHit* > >::iterator mapItr = cellHitMap.begin();
			mapItr != cellHitMap.end();
			++mapItr ){

		if( mapItr->second.size() == 1 ){      
			// only one hit in the cell

			const DBCALUnifiedHit* hit = mapItr->second[0];

			single_ended_hits.push_back(hit);

		}
	}

	vector<DBCALCluster*> clusters = clusterize( twoEndPoint, usedPoints,  single_ended_hits, tracks );

	// load our vector of clusters into the factory member data
	for( vector<DBCALCluster*>::iterator clust = clusters.begin();
			clust != clusters.end();
			++clust ){

		// put in an energy threshold for clusters
		if( (**clust).E() < 5*k_MeV ) {
			delete *clust;
			continue;
		}
		vector<const DBCALPoint*>points=(**clust).points();
		for (unsigned int i=0;i<points.size();i++){
		  (**clust).AddAssociatedObject(points[i]);
		}
		_data.push_back(*clust);
	}
	return NOERROR;
}

vector<DBCALCluster*>
DBCALCluster_factory::clusterize( vector< const DBCALPoint* > points , vector< const DBCALPoint* > usedPoints ,  vector< const DBCALUnifiedHit* > hits, vector< const DTrackWireBased* > tracks ) const {

	// first sort the points by energy
	sort( points.begin(), points.end(), PointSort );

	vector<DBCALCluster*> clusters(0);

	// ahh.. more hard coded numbers that should probably
	// come from a database or something
	float seedThresh = 1.*k_GeV;
	float minSeed = 10*k_MeV;
	//We have a big problem with noise in the outer layer of the detector
	//(the noise is the greatest in the outer layer, since the number of SiPMs
	//being summed is also the greatest here).
	//Thus there are a lot of DBCALPoint's in this layer that are pure noise hits.
	//The simplest way to deal with this is to prevent outer layer points
	//from seeding clusters. So hits in the outer layer can be associated
	//with existing clusters, but cannot create their own cluster.
	//This is okay since since isolated hits in the outer layer
	//is not really a signature we expect for many physical showers.
	//However, if a hit is sufficiently energetic, it is unlikely to be a noise
	//hit. For this reason, we allow 4th layer hits to seed clusters,
	//but we need a different (higher) minimum seed energy.
	float layer4_minSeed = 50*k_MeV;

	int counter = 0;
	int seed_counter = 0;
	int fit_counter = 0;

	float summed_r1 = 0.;
	float summed_phi1 = 0.;
	float summed_rphi1 = 0.;
	float summed_r_sq1 = 0.;

	float slope = 0.;
	float y_intercept = 0.;
	float tracked_phi = 0.;
	
	vector<pair < double , double > > point_reg;

	point_reg.clear();

	while( seedThresh > minSeed ) {

		bool usedPoint = false;

		for( vector< const DBCALPoint* >::iterator pt = points.begin();
				pt != points.end();
				++pt ){

			// first see if point should be added to an existing
			// cluster

			//cout << " point E = " << (**pt).E() << endl;
			int q = 0;
			double track_phi;	
			double track_phi_inner_r = 0.;		
			double closest_dPhi = 7.;
			DVector3 track_pos(0.0, 0.0, 0.0);
			DVector3 track_inner_rad(0.0,0.0,0.0);
			for( vector< const DTrackWireBased* >::iterator trk = tracks.begin();
				trk != tracks.end();
				++trk ){
				DVector3 temp_track_pos(0.0, 0.0, 0.0);
//				DVector3 temp_track_inner_rad(0.0,0.0,0.0);
				double point_r = (**pt).r();
				double point_z = (**pt).z();
				double point_theta_global = fabs(atan2(point_r,point_z + m_z_target_center ));
				(*trk)->rt->GetIntersectionWithRadius(point_r, temp_track_pos);
				(*trk)->rt->GetIntersectionWithRadius(64.3, track_inner_rad);
				if(track_inner_rad.Phi() >= 0.) track_phi_inner_r = track_inner_rad.Phi();
				else track_phi_inner_r = fabs(2*PI + track_inner_rad.Phi());
				if(track_pos.Phi() >= 0.) track_phi = temp_track_pos.Phi();
				else track_phi = fabs(2*PI + temp_track_pos.Phi()); 
				double dPhi = fabs((**pt).phi() - track_phi);
				if(dPhi < closest_dPhi){
					track_pos = temp_track_pos;
					closest_dPhi = dPhi;
				}
				double dTheta = fabs(point_theta_global - track_pos.Theta());
				if(dPhi < .175 && dTheta < .087 && track_pos.Perp() == point_r) q = 1;
			}

				double track_rho = sqrt(track_inner_rad.Perp()*track_inner_rad.Perp() + track_inner_rad.z()*track_inner_rad.z());

			cout << " track phi = " << track_phi_inner_r << " track r = " <<  track_inner_rad.Perp() << endl;

			for( vector<DBCALCluster*>::iterator clust = clusters.begin();
					clust != clusters.end();
					++clust ){

				for(vector< const DBCALPoint* >::iterator pt_o = points.begin();
                                	pt_o != points.end();
                                	++pt_o ){
					if((**clust).Q()==1){
						if(overlap_charged( **clust,*pt_o, track_inner_rad, slope, y_intercept, tracked_phi, counter) ){
							usedPoints.push_back( *pt_o );
							(**clust).addPoint( *pt_o );
							point_reg.push_back(make_pair( (**pt_o).r(), (**pt_o).phi() ) );
							points.erase( pt_o );
							cout << " charged success " << endl; 
//							cout << " clust E = " << (**clust).E() << " point E = " << (**pt_o).E() << " point M L S = " << (**pt_o).module() << "," << (**pt_o).layer() << "," << (**pt_o).sector() << endl;
							usedPoint = true;
						}
						if( usedPoint ) break;
					}			
				}
				if( usedPoint ) break;
				if( overlap( **clust, *pt ) ){
					if(BCALCLUSTERVERBOSE>0) cout << " overlap success " << endl;            
					usedPoints.push_back( *pt );  
					(**clust).addPoint( *pt );
					cout << " neut success " << endl;
					points.erase( pt );
					usedPoint = true;
				}

				// once we erase a point the iterator is no longer useful
				// and we start the loop over, so that a point doesn't get added to
				// multiple clusters. We will recycle through points later to 
				// check if a point was added to its closeset cluster.
				if( usedPoint ) break;
			}

			if( usedPoint ) break;
		
			for(unsigned int i = 0 ; i < point_reg.size() ; i++){
				summed_r1 += point_reg[i].first;
				summed_phi1 += point_reg[i].second;
				summed_rphi1 += point_reg[i].first*point_reg[i].second;
				summed_r_sq1 += (point_reg[i].first*point_reg[i].first);
//				cout << " i = " << i << " point r sum = " << summed_r1 << " point phi summed = " << summed_phi1 << endl; 
			}
			
//			if(seed_counter > 0 && fit_counter < 7){
			slope = (summed_r1*summed_phi1 - point_reg.size()*summed_rphi1)/(summed_r1*summed_r1 - point_reg.size()*summed_r_sq1);
			y_intercept = (summed_rphi1*summed_r1 - summed_phi1*summed_r_sq1)/(summed_r1*summed_r1 - point_reg.size()*summed_r_sq1);

//			cout << " SLOPE = " << slope << " y int = " << y_intercept << " r summed = " << summed_r1 << " phi summed = " << summed_phi1 << " summed rphi = " << summed_rphi1 << " summed rsq = " << summed_r_sq1 << " point size = " << point_reg.size() << endl;
//			if(seed_counter == 1) counter +=1;
//  				japp->WriteLock("DBCALCluster_factory");
//				charged_dist->Fit("charged_fit","Qsame");
//				japp->Unlock("DBCALCluster_factory");
				counter +=1;
	//			fit_counter +=1;
	//			break;
		//	}
			// if the point doesn't overlap with a cluster
			// see if it can become a new seed
			if( (**pt).E() > seedThresh && ((**pt).layer() != 4 || (**pt).E() > layer4_minSeed) ){
				clusters.push_back(new DBCALCluster( *pt, m_z_target_center, q ) );
				slope = (track_phi_inner_r - (**pt).phi())/(64.3 - (**pt).r());
				y_intercept = track_phi_inner_r - slope*track_rho;                         	
				tracked_phi = track_phi_inner_r;			
				points.erase( pt );
				usedPoint = true;
				seed_counter +=1 ;
//				cout << " seed point E = " << (**pt).E() << endl;
			}

			if( usedPoint ) break;
		}

		recycle_points( usedPoints, clusters);	
		// recycle through points that were added to a cluster and check if they
		// were added to their closest cluster. If they weren't then we remove 
		// the point from its original cluster and add it to its closest cluster.
	
//		merge( clusters );
		// lower the threshold to look for new seeds if none of 
		// the existing points were used as new clusters or assigned
		// to existing clusters
		if( !usedPoint ) seedThresh /= 2;
	}

	// add the single-ended hits that overlap with a cluster that was made from points
	for( vector< const DBCALUnifiedHit* >::iterator ht = hits.begin();
			ht != hits.end();
			++ht){
		bool usedHit = false;	 

		for( vector<DBCALCluster*>::iterator clust = clusters.begin();
				clust != clusters.end();
				++clust ){

			if( overlap( **clust, *ht ) ){

				int channel_calib = 16*((**ht).module-1)+4*((**ht).layer-1)+(**ht).sector-1; // need to use cellID for objects in DBCALGeometry but the CCDB uses a different channel numbering scheme, so use channel_calib when accessing CCDB tables.

				// given the location of the cluster, we need the best guess
				// for z with respect to target at this radius

				double z = (**clust).rho()*cos((**clust).theta()) + m_z_target_center;
				double d = ( ((**ht).end == 0) ? (z  - DBCALGeometry::GetBCAL_center() + DBCALGeometry::GetBCAL_length()/2.0) : (DBCALGeometry::GetBCAL_center() + DBCALGeometry::GetBCAL_length()/2.0 - z));  // d gives the distance to upstream or downstream end of BCAL depending on where the hit was with respect to the cluster z position.
				double lambda = attenuation_parameters[channel_calib][0];
				double hit_E = (**ht).E;
				double hit_E_unattenuated = hit_E/exp(-d/lambda);  // hit energy unattenuated wrt the cluster z position

				(**clust).addHit( *ht, hit_E_unattenuated );
				usedHit = true;
			}
			if( usedHit ) break;
		}
	}     
	return clusters;
}

void
DBCALCluster_factory::recycle_points( vector<const DBCALPoint*> usedPoints, vector<DBCALCluster*>& clusters) const{

	vector<double> sep_vector;
	vector<double> deltPhi;
	vector<double>::const_iterator min_sep;
	vector<double>::const_iterator min_phi;
        
	if ( clusters.size() <= 1 ) return;
	
	sort( clusters.begin(), clusters.end(), ClusterSort );

	for( vector<const DBCALPoint*>::const_iterator usedpt = usedPoints.begin();
		usedpt != usedPoints.end();
		++usedpt ){		
		
		int overlap_counter = 0;				
		sep_vector.clear();
		deltPhi.clear();
		double sep = 0.;
		
		for( vector<DBCALCluster*>::iterator clust = clusters.begin();
			clust != clusters.end();
			++clust ){
	
			if( overlap( **clust, *usedpt ) ){
				overlap_counter++;
				// If a point satisfies the overlap condition with a cluster, then we want to calculate the
				// distance along the sphere between the point and cluster centroid position.
				float deltaTheta = fabs( (**clust).theta() - (*usedpt)->theta() );
				float deltaPhi = (**clust).phi() - (*usedpt)->phi();
        			float deltaPhiAlt = ( (**clust).phi() > (*usedpt)->phi() ?
                        	(**clust).phi() - (*usedpt)->phi() - 2*PI :
                        	(*usedpt)->phi() - (**clust).phi() - 2*PI );

        			deltaPhi = min( fabs( deltaPhi ), fabs( deltaPhiAlt ) );
					
				deltPhi.push_back(deltaPhi);

				float rho = ( (**clust).rho() + (*usedpt)->rho() ) / 2.;
	        		float theta = ( (**clust).theta() + (*usedpt)->theta() ) / 2.;
	
				sep =  sqrt( ( rho * deltaTheta ) * ( rho * deltaTheta ) + ( rho * sin( theta ) * deltaPhi ) * ( rho * sin( theta ) * deltaPhi ) );
				sep_vector.push_back(sep);

			}
						
		}
	
		if(overlap_counter==0) break;
			
		min_sep = min_element(sep_vector.begin(),sep_vector.end());
		min_phi = min_element(deltPhi.begin(),deltPhi.end());
		// Find the points closest cluster in distance along the sphere and in phi.			

		for( vector<DBCALCluster*>::iterator clust = clusters.begin();
			clust != clusters.end();
			++clust ){
			bool sep_match;
			bool clust_match;
			bool point_match;
			int best_clust = 0;
			vector<const DBCALPoint*>associated_points=(**clust).points();
			float deltaTheta = fabs( (**clust).theta() - (*usedpt)->theta() );
			float deltaPhi = (**clust).phi() - (*usedpt)->phi();
			float deltaPhiAlt = ( (**clust).phi() > (*usedpt)->phi() ?
			(**clust).phi() - (*usedpt)->phi() - 2*PI :
			(*usedpt)->phi() - (**clust).phi() - 2*PI );

			deltaPhi = min( fabs( deltaPhi ), fabs( deltaPhiAlt ) );

			float rho = ( (**clust).rho() + (*usedpt)->rho() ) / 2.;
			float theta = ( (**clust).theta() + (*usedpt)->theta() ) / 2.;

			sep =  sqrt( ( rho * deltaTheta ) * ( rho * deltaTheta ) + ( rho * sin( theta ) * deltaPhi ) * ( rho * sin( theta ) * deltaPhi ) );
				
			for(unsigned int j = 0 ; j < associated_points.size(); j++){
				// Check to see if the point we are comparing to the cluster
				// is already in that cluster.
				sep_match = ((sep > *min_sep-.1) && ( sep < *min_sep+.1));
				clust_match = ((*usedpt)->E() == associated_points[j]->E());
				if ( (deltaPhi==*min_phi) && clust_match==1) best_clust=1;
				if(BCALCLUSTERVERBOSE>1)cout << " clust E = " << (**clust).E() <<" assoc point E = " << associated_points[j]->E() << " points E = " << (*usedpt)->E() <<  " clust match = " << clust_match <<" sep = " << sep << " min sep = " << *min_sep << " sep match = " << sep_match <<  endl;
			}
			if(best_clust==1) break;
			// if the point was originally placed in its "best" cluster then we don't want to touch it.
			if(best_clust==0){
				int added_point = 0;
				int removed_point = 0;
				for(unsigned int i = 0 ; i < associated_points.size(); i++){
					point_match = ((*usedpt)->E() == associated_points[i]->E());
					if( point_match==0 && added_point==0 && deltaPhi == *min_phi){
						(**clust).addPoint( *usedpt );
						// if the point found a closer cluster then we add it to the closer cluster.
						// The point is now an associated object of the closer cluster.
						added_point=1;
					}
                                	if( point_match==1 && removed_point==0 && deltaPhi != *min_phi){
						(**clust).removePoint( *usedpt );
						// Now we remove the point from its original cluster since it has been added
						// to its closest cluster. The point is no longer an associated object of
						// the original cluster.
						removed_point=1;
					}
				}
			}   
		}
	}
}	

void
DBCALCluster_factory::merge( vector<DBCALCluster*>& clusters ) const {

	if( clusters.size() <= 1 ) return;

	sort( clusters.begin(), clusters.end(), ClusterSort );

	bool stillMerging = true;

	while( stillMerging ){

		stillMerging = false;
		for( vector<DBCALCluster*>::iterator hClust = clusters.begin();
				hClust != clusters.end() - 1;
				++hClust ){

			for( vector<DBCALCluster*>::iterator lClust = hClust + 1;
					lClust != clusters.end();
					++lClust ){

				if( overlap( **hClust, **lClust ) ){

					(**hClust).mergeClust(**lClust);
					delete *lClust;
					clusters.erase( lClust );

					// now iterators are invalid and we need to bail out of loops
					stillMerging = true;
					break;
				}
			}
			if( stillMerging ) break;
		}
	}
}

bool
DBCALCluster_factory::overlap( const DBCALCluster& highEClust,
		const DBCALCluster& lowEClust ) const {

	float sigTheta = fabs( highEClust.theta() - lowEClust.theta() ) / 
		sqrt( highEClust.sigTheta() * highEClust.sigTheta() +
				lowEClust.sigTheta()  * lowEClust.sigTheta() );

	// difference in phi is tricky due to overlap at 0/2pi
	// order based on phi and then take the minimum of the difference
	// and the difference with 2pi added to the smallest

	float deltaPhi = highEClust.phi() - lowEClust.phi();
	float deltaPhiAlt = ( highEClust.phi() > lowEClust.phi() ? 
			highEClust.phi() - lowEClust.phi() - 2*PI :
			lowEClust.phi() - highEClust.phi() - 2*PI );

	deltaPhi = min( fabs( deltaPhi ), fabs( deltaPhiAlt ) );

	float sigPhi = deltaPhi / 
		sqrt( highEClust.sigPhi() * highEClust.sigPhi() +
				lowEClust.sigPhi()  * lowEClust.sigPhi() );

	//We can't rely entirely on sigTheta and sigPhi as defined above.
	//For high-energy clusters, the position uncertainties will be very small,
	//so sigTheta/sigPhi will be large, and clusters may not merge properly.
	//To fix this, force clusters to merge if delta_z and delta_phi are both
	//very small. This is hopefully only a temporary fix.

	//deltaPhi_force_merge and delta_z_force_merge were determined by looking
	//at the separation of decay photons from pi0's from a pythia sample.
	//There are no events where the decay photons have separation
	//(delta_phi < 0.2 && delta_z < 25 cm), so in most cases it should be safe
	//to merge clusters together if they are so close.
	const double deltaPhi_force_merge = 0.1; //radians
	const double delta_z_force_merge = 15.0*k_cm;

	//A major cause of extra clusters are lower energy hits, which have poor
	//z-resolution and so are not properly merged. Treat low energy
	//clusters (< 40 MeV) as a special case. Again, hopefully this is only
	//a temporary fix until we have a more comprehensive solution.
	const double delta_z_force_merge_low_E = 40.0*k_cm;
	const double low_E = .04*k_GeV;

	double z1 = DBCALGeometry::GetBCAL_inner_rad()/tan(highEClust.theta());
	double z2 = DBCALGeometry::GetBCAL_inner_rad()/tan(lowEClust.theta());
	double delta_z = fabs(z1-z2);

	bool theta_match = (sigTheta < m_mergeSig) || (delta_z < delta_z_force_merge) || (delta_z < delta_z_force_merge_low_E && lowEClust.E() < low_E);

	bool phi_match = (sigPhi < m_mergeSig) || (deltaPhi < deltaPhi_force_merge);

	//very loose cut to check that the two clusters are in time
	bool time_match = (highEClust.t() - lowEClust.t()) < m_timeCut;

	if(BCALCLUSTERVERBOSE>1) cout << " clust merge: " << " theta match success = " << theta_match << " phi match = " << phi_match << " time match = " << time_match << " high E = " << highEClust.E() << " low E = " << lowEClust.E() << " highE z = " << z1 << " lowE z = " << z2 << " deltaTheta = " << fabs(highEClust.theta()-lowEClust.theta()) << " sigTheta = " << sigTheta << " highE sigTheta = " << highEClust.sigTheta() << " lowE sigTheta = " << lowEClust.sigTheta() << endl;

	return theta_match && phi_match && time_match;

}

bool
DBCALCluster_factory::overlap( const DBCALCluster& clust,
		const DBCALPoint* point ) const {


	float deltaTheta = fabs( clust.theta() - point->theta() );
	/* sigTheta not used
	   float sigTheta = deltaTheta / sqrt( clust.sigTheta() * clust.sigTheta() +
	   point->sigTheta()  * point->sigTheta() );
	   */

	// difference in phi is tricky due to overlap at 0/2pi
	// order based on phi and then take the minimum of the difference
	// and the difference with 2pi added to the smallest

	float deltaPhi = clust.phi() - point->phi();
	float deltaPhiAlt = ( clust.phi() > point->phi() ? 
			clust.phi() - point->phi() - 2*PI :
			point->phi() - clust.phi() - 2*PI );

	deltaPhi = min( fabs( deltaPhi ), fabs( deltaPhiAlt ) );

	/* sigPhi not used
	   float sigPhi = deltaPhi / 
	   sqrt( clust.sigPhi() * clust.sigPhi() +
	   point->sigPhi()  * point->sigPhi() );
	   */

	float rho = ( clust.rho() + point->rho() ) / 2;
	float theta = ( clust.theta() + point->theta() ) / 2;

	float sep = sqrt( ( rho * deltaTheta ) * ( rho * deltaTheta ) +
			( rho * sin( theta ) * deltaPhi ) * ( rho * sin( theta ) * deltaPhi ) );

	float sep_term1 = rho*deltaTheta;
	float sep_term2 = rho*sin(theta)*deltaPhi;

	//very loose cuts to make sure the two hits are in time
	bool time_match = fabs(clust.t() - point->t()) < m_timeCut;

	double clust_z = clust.rho()*cos(clust.theta());

	//double c1 = C1_parm->Eval(clust_z);
	double c1=23.389+19.093*tanh(-0.0104*(clust_z-201.722));

	//double c2 = C2_parm->Eval(clust_z);
	double c2=0.151+0.149*tanh(-0.016*(clust_z-275.194));

	//dtheta_inclusion_curve->SetParameter(0,c1);
	//dtheta_inclusion_curve->SetParameter(1,c2); 
	
	//double inclusion_val = sep_inclusion_curve->Eval(sep);
	double inclusion_val=exp(-sep/30.)-0.1;

        //double inclusion_val1 = dtheta_inclusion_curve->Eval(sep_term1);
	double inclusion_val1=exp(-(sep_term1-0.1)/c1)-c2+.15;
	
        //double inclusion_val2 = dphi_inclusion_curve->Eval(sep_term2);	
	double inclusion_val2=exp(-(sep_term2-2.)/2.5)-sep_term2*0.002+0.07;
	
	// We consider fractional energy (point.E/Clust.E) as a function of spatial separation between
	// a point and cluster to determine if the point should be included in the cluster.
	// These distributions are tighter in the phihat direction than along thetahat. For more details
	// on how the selection criteria for cluster,point overlap function go to logbook entry 3396018.	

	if(BCALCLUSTERVERBOSE>0) cout << "(m,l,s) = (" <<point->module()<<","<<point->layer()<<","<<point->sector()<<")" <<  " sep = " << sep << "sep1 = " << sep_term1 << " sep2 = " << sep_term2 << " inclusion value = " << inclusion_val << " inclusion val1= " << inclusion_val1 << " inclusion val2= " << inclusion_val2<< " time match = " << time_match << " clust E = " << clust.E() << " point E = " << point->E() << " energy ratio = " << point->E()/(point->E()+clust.E()) <<  " clust theta = " << clust.theta()*180./3.14159 << " point theta = " << point->theta()*180./3.14159 << " sep rho*deltaTheta = " << ( rho * deltaTheta ) << endl;

/*	if(sep>m_moliereRadius && sep<7.*m_moliereRadius &&sep_term2>=2.*m_moliereRadius){
		return ((point->E()/(point->E()+clust.E())) < inclusion_val1) && ((point->E()/(point->E()+clust.E())) < inclusion_val2) && time_match && deltaPhi*180./3.14159<10.;
	}
*/
	if(sep>m_moliereRadius && sep<7.*m_moliereRadius &&sep_term2>=2.*m_moliereRadius){
                return ((point->E()/(point->E()+clust.E())) < inclusion_val1 ) && ((point->E()/(point->E()+clust.E())) < inclusion_val2 ) && time_match && deltaPhi*180./3.14159<10.;
        }

	else{
		return ((point->E()/(point->E()+clust.E())) < (inclusion_val1+.2)) && sep < 10.*m_moliereRadius && time_match && sep_term2<2.*m_moliereRadius;
	}

}


bool
DBCALCluster_factory::overlap_charged( const DBCALCluster& clust,
		const DBCALPoint* point, DVector3 track_pos, float slope, float y_intercept, float tracked_phi, int counter ) const {


	/* sigTheta not used
	   float sigTheta = deltaTheta / sqrt( clust.sigTheta() * clust.sigTheta() +
	   point->sigTheta()  * point->sigTheta() );
	   */

	// difference in phi is tricky due to overlap at 0/2pi
	// order based on phi and then take the minimum of the difference
	// and the difference with 2pi added to the smallest

	vector<const DBCALPoint*> assoc_points;
//	assoc_points.clear();
	assoc_points = (clust).points();

//	vector<const DBCALPoint*> assoc_points =(clust).points();

//	assoc_points.clear();

	double summed_r0 = 0.;
	double summed_phi0 = 0.;
	double summed_rphi0 = 0.;
	double summed_r_sq0 = 0.;
	double summed_r = 0.;
        double summed_phi = 0.;
        double summed_rphi = 0.;
        double summed_r_sq = 0.;
	
	double slope2 = 0.;
	double y_intercept2 = 0.;
	int sign = 0;
	int signAlt = 0;
	double p_phi = 0.;

	for(unsigned int i = 0 ; i < assoc_points.size() ; i ++){
		summed_r0 += assoc_points[i]->r();
		double del_phi = assoc_points[i]->phi() - tracked_phi;
		sign = 0;
		signAlt = 0;
		if(del_phi < 0) sign = -1;
		if(del_phi > 0) sign = 1;
		double del_phiAlt = ( tracked_phi  < assoc_points[i]->phi() ?
                         assoc_points[i]->phi() - tracked_phi - 2*PI :
                        -assoc_points[i]->phi() + tracked_phi - 2*PI );
		if(del_phiAlt < 0) signAlt = -1;
		if(del_phiAlt > 0) signAlt = 1;
		cout << " del phi = " << del_phi << " del phi alt = " << del_phiAlt << endl;
		del_phi = min( fabs(del_phi), fabs(del_phiAlt) );
		if( del_phi == fabs(del_phi) && sign == 1) p_phi = del_phi + tracked_phi;
		if( del_phi == fabs(del_phi) && sign == -1) p_phi = -del_phi + tracked_phi;
		if( del_phi == fabs(del_phiAlt) && signAlt == 1) p_phi = del_phiAlt + tracked_phi;
                if( del_phi == fabs(del_phiAlt) && signAlt == -1) p_phi = -del_phiAlt + tracked_phi;
		//cout << " min phi = " << p_phi << " point phi = " << assoc_points[i]->phi() << " sign = " << sign << " signalt = " << signAlt << endl;
		summed_phi0 += p_phi;
		summed_rphi0 += assoc_points[i]->r()*p_phi;
		summed_r_sq0 += assoc_points[i]->r()*assoc_points[i]->r();
//		cout << " overlap i = " << i << " point r = " << assoc_points[i]->r() << " point r sum = " << summed_r0 << " point phi = " << assoc_points[i]->phi()*TMath::RadToDeg() << " point phi sum = " << summed_phi0*TMath::RadToDeg() << " point E = " << assoc_points[i]->E() << endl;
	}


/*	summed_r = summed_r0 + 64.3 ;
	summed_phi = summed_phi0 + tracked_phi ;
	summed_rphi = summed_rphi0 + 64.3*tracked_phi ;
	summed_r_sq = summed_r_sq0 + 64.3*64.3 ;
*/

	summed_r = summed_r0 ;
        summed_phi = summed_phi0 ;
        summed_rphi = summed_rphi0 ;
        summed_r_sq = summed_r_sq0  ;
/*
	double lin_reg_m = (r_summed*phi_summed - locBCALPoints.size()*phi_x_r_summed)/(r_summed*r_summed - locBCALPoints.size()*r_sq_summed);
        double lin_reg_b = (phi_x_r_summed*r_summed - phi_summed*r_sq_summed)/(r_summed*r_summed - locBCALPoints.size()*r_sq_summed);
*/

	double track_rho = sqrt(track_pos.Perp()*track_pos.Perp() + track_pos.z()*track_pos.z());

	if(assoc_points.size()<2){
		slope2 = (tracked_phi - summed_phi0)/(64.3 - summed_r0);
                y_intercept2 = tracked_phi - slope2*64.3;
		cout << " tracked phi = " << tracked_phi << " point phi = " << point->phi() << " track pos perp = " << track_pos.Perp() << " point r = " << point->r() << " slope = 2 " << slope2 << " y int = " << y_intercept2 << endl;

	}

        else{
		slope2 = (summed_r*summed_phi - assoc_points.size()*summed_rphi)/(summed_r*summed_r - assoc_points.size()*summed_r_sq);
        	y_intercept2 = (summed_rphi*summed_r - summed_phi*summed_r_sq)/(summed_r*summed_r - assoc_points.size()*summed_r_sq);
	}


//	double lin_reg_m = (r_summed*phi_summed - locBCALPoints.size()*phi_x_r_summed)/(r_summed*r_summed - locBCALPoints.size()*r_sq_summed);

	double slope_num = ( (summed_r+point->r())*(summed_phi*TMath::RadToDeg()+point->phi()*TMath::RadToDeg()) - (assoc_points.size()+1)*(summed_rphi*TMath::RadToDeg() + point->r()*point->phi()*TMath::RadToDeg()) ); 
	double slope_denom = ( (summed_r+point->r())*(summed_r+point->r()) - (assoc_points.size()+1)*(summed_r_sq + point->r()*point->r()) );
	double slope_div = slope_num / slope_denom;

//	cout << " slope div = " << slope_div << " slope num = " << slope_num << " slope demon = " << slope_denom << " summed r = " << summed_r << " point size = " << assoc_points.size() << " summed r sq = " << summed_r_sq << " point r sq = " << point->r()*point->r() << endl; 

/*	double lin_reg_m = (r_summed*phi_summed - locBCALPoints.size()*phi_x_r_summed)/(r_summed*r_summed - locBCALPoints.size()*r_sq_summed);
        double lin_reg_b = (phi_x_r_summed*r_summed - phi_summed*r_sq_summed)/(r_summed*r_summed - locBCALPoints.size()*r_sq_summed);
*/

	cout << " slope2 = " << slope2 << " y int 2 = " << y_intercept2 << " point r = " << point->r() << " assoc point size = " << assoc_points.size() <<  endl;

//	cout << " slope overlap = " << slope2*TMath::RadToDeg() << " slope ize = " << slope*TMath::RadToDeg() << " y int overlap = " << y_intercept2*TMath::RadToDeg() << " y int ize = " << y_intercept*TMath::RadToDeg() << " r sum = " << summed_r << " summed phi = " << summed_phi*TMath::RadToDeg() << endl;

	float fit_phi = 0.;
	float fit_phi2 = 0.;

	fit_phi2 = slope*point->r() + y_intercept;

	assoc_points.clear();

//	fit_phi = charged_fit->Eval(point->r());

	fit_phi = slope2*point->r() + y_intercept2;
//	fit_phi2 = slope2*point->r() + y_intercept2;
//	else fit_phi = charged_fit->Eval(point->r());

//	cout << " fit phi eval = " << charged_fit->Eval(point->r()) << " fit phi = " << fit_phi << " point phi = " << point->phi() << endl;

//	float fit_phi = charged_fit->Eval(point->r());
	float deltaPhi = fit_phi-point->phi();
	float deltaPhiAlt = ( fit_phi  > point->phi() ? 
                        fit_phi  - point->phi() - 2*PI :
                        point->phi() - fit_phi - 2*PI );

	deltaPhi = min( fabs( deltaPhi ), fabs( deltaPhiAlt ) );

	/* sigPhi not used
	   float sigPhi = deltaPhi / 
	   sqrt( clust.sigPhi() * clust.sigPhi() +
	   point->sigPhi()  * point->sigPhi() );
	   */

/*	float track_z = track_pos.z()-m_z_target_center;
	float track_rho = sqrt( track_pos.Perp()*track_pos.Perp() + track_z*track_z );
	float rho = ( track_rho + point->rho() ) / 2.;
	float track_theta = fabs(atan2(track_pos.Perp(), track_z ));
	float theta = ( track_theta + point->theta() ) / 2.;
*/
	float rho = point->rho();
	float theta = point->theta();

	float deltaTheta = fabs( clust.theta() - point->theta() );

	float sep = sqrt( ( rho * deltaTheta ) * ( rho * deltaTheta ) +
			( rho * sin( theta ) * deltaPhi ) * ( rho * sin( theta ) * deltaPhi ) );

	float sep_term1 = rho*deltaTheta;
	float sep_term2 = rho*sin(theta)*deltaPhi;

//	cout << " fit phi = " << fit_phi*TMath::RadToDeg() << " fit phi 2 = " << fit_phi2*TMath::RadToDeg() <<  " clut phi = " << clust.phi()*TMath::RadToDeg() << " delta phi = " << deltaPhi << " sep2 = " << sep_term2 << endl;

	//very loose cuts to make sure the two hits are in time
	bool time_match = fabs(clust.t() - point->t()) < m_timeCut;

	double clust_z = clust.rho()*cos(clust.theta());;

	//double c1 = C1_parm->Eval(clust_z);
	double c1=23.389+19.093*tanh(-0.0104*(clust_z-201.722));

	//double c2 = C2_parm->Eval(clust_z);
	double c2=0.151+0.149*tanh(-0.016*(clust_z-275.194));

	//dtheta_inclusion_curve->SetParameter(0,c1);
	//dtheta_inclusion_curve->SetParameter(1,c2); 
	
	//double inclusion_val = sep_inclusion_curve->Eval(sep);
	double inclusion_val=exp(-sep/30.)-0.1;

        //double inclusion_val1 = dtheta_inclusion_curve->Eval(sep_term1);
	double inclusion_val1=exp(-(sep_term1-0.1)/c1)-c2+.15;
	
        //double inclusion_val2 = dphi_inclusion_curve->Eval(sep_term2);	
	double inclusion_val2=exp(-(sep_term2-2.)/2.5)-sep_term2*0.002+0.07;
	double inclusion_val2_L1 = exp(-(sep_term2-2.)/2.5) - sep_term2*.002 + 0.1;
	double inclusion_val2_L2 = exp(-(sep_term2-2.)/2.5) - sep_term2*.002 + 0.13;
	double inclusion_val2_L3 = exp(-(sep_term2-2.)/2.5) - sep_term2*.002 + 0.17;
	double inclusion_val2_L4 = exp(-(sep_term2-12.)/2.5) - sep_term2*.002 + 0.07;
	
	// We consider fractional energy (point.E/Clust.E) as a function of spatial separation between
	// a point and cluster to determine if the point should be included in the cluster.
	// These distributions are tighter in the phihat direction than along thetahat. For more details
	// on how the selection criteria for cluster,point overlap function go to logbook entry 3396018.	

	cout << " (m,l,s) = (" <<point->module()<<","<<point->layer()<<","<<point->sector()<<")" <<  " sep = " << sep << "sep1 = " << sep_term1 << " sep2 = " << sep_term2 << " inclusion value = " << inclusion_val << " inclusion val1= " << inclusion_val1 << " inclusion val2= " << inclusion_val2<< " clust E = " << clust.E() << " point E = " << point->E() << " energy ratio = " << point->E()/(point->E()+clust.E()) << " clust theta = " << clust.theta() << " point theta = " << point->theta() << " clust phi = " << clust.phi()*TMath::RadToDeg() << " fit phi = " << fit_phi*TMath::RadToDeg() << " point phi = " << point->phi()*TMath::RadToDeg() << " clust rho = " << clust.rho() << " counter = " << counter <<  endl;

//	if(BCALCLUSTERVERBOSE>0) cout << "(m,l,s) = (" <<point->module()<<","<<point->layer()<<","<<point->sector()<<")" <<  " sep = " << sep << "sep1 = " << sep_term1 << " sep2 = " << sep_term2 << " inclusion value = " << inclusion_val << " inclusion val1= " << inclusion_val1 << " inclusion val2= " << inclusion_val2<< " time match = " << time_match << " clust E = " << clust.E() << " point E = " << point->E() << " energy ratio = " << point->E()/(point->E()+clust.E()) <<  " clust theta = " << clust.theta()*180./3.14159 << " point theta = " << point->theta()*180./3.14159 << " sep rho*deltaTheta = " << ( rho * deltaTheta ) << endl;

//	if(BCALCLUSTERVERBOSE>0) 
//	cout << "(m,l,s) = (" <<point->module()<<","<<point->layer()<<","<<point->sector()<<")" <<  " sep = " << sep << "sep1 = " << sep_term1 << " sep2 = " << sep_term2 << " fit phi = " << fit_phi << " phi point = " << point->phi() << " delta phi = " << deltaPhi << " rho = " << rho << " theta = " << theta << " inclusion value = " << inclusion_val << " inclusion val1= " << inclusion_val1 << " inclusion val2= " << inclusion_val2<< " clust E = " << clust.E() << " point E = " << point->E() << endl;

/*	if(sep>m_moliereRadius && sep<7.*m_moliereRadius &&sep_term2>=2.*m_moliereRadius){
		return ((point->E()/(point->E()+clust.E())) < inclusion_val1) && ((point->E()/(point->E()+clust.E())) < inclusion_val2) && time_match && deltaPhi*180./3.14159<10.;
	}
*/
/*	if(sep>m_moliereRadius && sep<7.*m_moliereRadius &&sep_term2>=2.*m_moliereRadius){
		if(point->layer()==1) return ((point->E()/(point->E()+clust.E())) < inclusion_val1 + 50.) && ((point->E()/(point->E()+clust.E())) < inclusion_val2_L1 - 5.) && time_match;
		if(point->layer()==2) return ((point->E()/(point->E()+clust.E())) < inclusion_val1 + 50.) && ((point->E()/(point->E()+clust.E())) < inclusion_val2_L2 - 5.) && time_match;
		if(point->layer()==3) return ((point->E()/(point->E()+clust.E())) < inclusion_val1 + 50.) && ((point->E()/(point->E()+clust.E())) < inclusion_val2_L3 - 5.) && time_match;
		if(point->layer()==4) return ((point->E()/(point->E()+clust.E())) < inclusion_val1 + 50.) && ((point->E()/(point->E()+clust.E())) < inclusion_val2_L4 - 5.) && time_match;
	}

	else{
		return ((point->E()/(point->E()+clust.E())) < (inclusion_val1+.2)) && sep < 10.*m_moliereRadius && time_match && sep_term2<2.*m_moliereRadius;
	}
*/
	if(sep>m_moliereRadius && sep<7.*m_moliereRadius &&sep_term2>=2.*m_moliereRadius){
                return ((point->E()/(point->E()+clust.E())) < (inclusion_val1) ) && ((point->E()/(point->E()+clust.E())) < (inclusion_val2) ) && time_match && deltaPhi*180./3.14159<10.;
        }

        else{
                return ((point->E()/(point->E()+clust.E())) < (inclusion_val1+2.)) && sep < 10.*m_moliereRadius && time_match && sep_term2<2.*m_moliereRadius;
        }



}


bool
DBCALCluster_factory::overlap( const DBCALCluster& clust,
		const DBCALUnifiedHit* hit ) const {

	int cellId = DBCALGeometry::cellId( hit->module, hit->layer, hit->sector );

	float cellPhi = DBCALGeometry::phi( cellId );
	float cellSigPhi = DBCALGeometry::phiSize( cellId );

	// annoying +- 2pi business to try to find the best delta phi

	float deltaPhi = clust.phi() - cellPhi;
	float deltaPhiAlt = ( clust.phi() > cellPhi ? 
			clust.phi() - cellPhi - 2*PI :
			cellPhi - clust.phi() - 2*PI );
	deltaPhi = min( fabs( deltaPhi ), fabs( deltaPhiAlt ) );  

	float sigPhi = deltaPhi / 
		sqrt( clust.sigPhi() * clust.sigPhi() + cellSigPhi  * cellSigPhi );

	int channel_calib = 16*(hit->module-1)+4*(hit->layer-1)+hit->sector-1; // need to use cellID for objects in DBCALGeometry but the CCDB uses a different channel numbering scheme, so use channel_calib when accessing CCDB tables.
	// given the location of the cluster, we need the best guess
	// for z with respect to target at this radius
	double z = clust.rho()*cos(clust.theta()) + m_z_target_center;        
	double d = ( (hit->end == 0) ? (z - DBCALGeometry::GetBCAL_center() + DBCALGeometry::GetBCAL_length()/2.0) : (DBCALGeometry::GetBCAL_center() + DBCALGeometry::GetBCAL_length()/2.0 - z));  // d gives the distance to upstream or downstream end of BCAL depending on where the hit was with respect to the cluster z position.
	double time_corr = hit->t - d/effective_velocities[channel_calib];  // hit time corrected to the interaction point in the bar.        
	double time_diff = TMath::Abs(clust.t() - time_corr); // time cut between cluster time and hit time - 20 ns is a very loose time cut.

	return( sigPhi < m_mergeSig && time_diff < m_clust_hit_timecut ); 

}
