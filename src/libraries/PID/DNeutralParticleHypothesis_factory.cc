// $Id$
//
//    File: DNeutralParticleHypothesis_factory.cc
// Created: Thu Dec  3 17:27:55 EST 2009
// Creator: pmatt (on Linux ifarml6 2.6.18-128.el5 x86_64)
//


#include <iostream>
#include <iomanip>
using namespace std;

#include <TMath.h>

#include "DNeutralParticleHypothesis_factory.h"
using namespace jana;

//------------------
// init
//------------------
jerror_t DNeutralParticleHypothesis_factory::init(void)
{
	return NOERROR;
}

//------------------
// brun
//------------------
jerror_t DNeutralParticleHypothesis_factory::brun(jana::JEventLoop *locEventLoop, int runnumber)
{
	// Get Target parameters from XML
	DApplication *locApplication = dynamic_cast<DApplication*> (locEventLoop->GetJApplication());
	DGeometry *locGeometry = locApplication ? locApplication->GetDGeometry(runnumber):NULL;
	dTargetLength = 30.0;
	double locTargetCenterZ = 65.0;
	dTargetRadius = 1.5; //FIX: grab from database!!!
	if(locGeometry)
	{
		locGeometry->GetTargetZ(locTargetCenterZ);
		locGeometry->GetTargetLength(dTargetLength);
	}
	dTargetCenter.SetXYZ(0.0, 0.0, locTargetCenterZ);

	return NOERROR;
}

//------------------
// evnt
//------------------
jerror_t DNeutralParticleHypothesis_factory::evnt(jana::JEventLoop *locEventLoop, int eventnumber)
{
	unsigned int locNDF = 1;
	float locMass, locPMag, locShowerEnergy, locParticleEnergy, locPathLength, locHitTime, locFlightTime, locProjectedTime, locTimeDifference;
	float locParticleEnergyUncertainty, locShowerEnergyUncertainty, locTimeDifferenceVariance, locChiSq, locFOM;
	DVector3 locPath, locHitPoint, locMomentum;

	const DNeutralShower *locNeutralShower;
	DNeutralParticleHypothesis *locNeutralParticleHypothesis;
	DMatrixDSym locParticleCovariance;

	vector<const DNeutralShower*> locNeutralShowers;
	locEventLoop->Get(locNeutralShowers);

	vector<Particle_t> locPIDHypotheses;
	locPIDHypotheses.push_back(Gamma);
	locPIDHypotheses.push_back(Neutron);

	vector<const DEventRFBunch*> locEventRFBunches;
	locEventLoop->Get(locEventRFBunches);
	const DEventRFBunch* locEventRFBunch = locEventRFBunches[0];

	double locStartTime = locEventRFBunch->dMatchedToTracksFlag ? locEventRFBunch->dTime : numeric_limits<double>::quiet_NaN();
	DLorentzVector locSpacetimeVertex(dTargetCenter, locStartTime);
	double locStartTimeVariance = locEventRFBunch->dTimeVariance;

	// Loop over DNeutralShowers
	for (unsigned int loc_i = 0; loc_i < locNeutralShowers.size(); loc_i++){
		locNeutralShower = locNeutralShowers[loc_i];

		locHitTime = locNeutralShower->dSpacetimeVertex.T();
		locShowerEnergy = locNeutralShower->dEnergy;
		locShowerEnergyUncertainty = sqrt(locNeutralShower->dCovarianceMatrix(0, 0));
		locHitPoint = locNeutralShower->dSpacetimeVertex.Vect();

		// Loop over vertices and PID hypotheses & create DNeutralParticleHypotheses for each combination
		for (unsigned int loc_k = 0; loc_k < locPIDHypotheses.size(); loc_k++)
		{
			// Calculate DNeutralParticleHypothesis Quantities (projected time at vertex for given id, etc.)
			locMass = ParticleMass(locPIDHypotheses[loc_k]);
			locParticleEnergy = locShowerEnergy; //need to correct this for neutrons!
			if (locParticleEnergy < locMass)
				continue; //not enough energy for PID hypothesis

			locParticleEnergyUncertainty = locShowerEnergyUncertainty; //need to correct this for neutrons!

			locPath = locHitPoint - locSpacetimeVertex.Vect();
			locPathLength = locPath.Mag();
			if(!(locPathLength > 0.0))
				continue; //invalid, will divide by zero when creating error matrix, so skip!

			locPMag = sqrt(locParticleEnergy*locParticleEnergy - locMass*locMass);
			locFlightTime = locPathLength*locParticleEnergy/(locPMag*SPEED_OF_LIGHT);
			locProjectedTime = locHitTime - locFlightTime;
			locMomentum = locPath;
			locMomentum.SetMag(locPMag);

			Calc_ParticleCovariance(locNeutralShower, locMass, locMomentum, locPath, locParticleCovariance);

			// Calculate DNeutralParticleHypothesis FOM
			locTimeDifference = locSpacetimeVertex.T() - locProjectedTime;
			locTimeDifferenceVariance = locParticleCovariance(6, 6) + locStartTimeVariance;
			locChiSq = locTimeDifference*locTimeDifference/locTimeDifferenceVariance;
			locFOM = TMath::Prob(locChiSq, locNDF);
			if(locPIDHypotheses[loc_k] == Neutron)
				locFOM = -1.0; //disables neutron ID until the neutron energy is calculated correctly from the deposited energy in the shower

			// Build DNeutralParticleHypothesis // dEdx not set
			locNeutralParticleHypothesis = new DNeutralParticleHypothesis;
			locNeutralParticleHypothesis->AddAssociatedObject(locNeutralShower);

			locNeutralParticleHypothesis->setMass(locMass);
			locNeutralParticleHypothesis->setCharge(0.0);
			locNeutralParticleHypothesis->setMomentum(locMomentum);
			locNeutralParticleHypothesis->setPosition(locSpacetimeVertex.Vect());
			locNeutralParticleHypothesis->setT0(locSpacetimeVertex.T(), sqrt(locStartTimeVariance), SYS_NULL);
			locNeutralParticleHypothesis->setTime(locProjectedTime);
			locNeutralParticleHypothesis->setT1(locNeutralShower->dSpacetimeVertex.T(), sqrt(locNeutralShower->dCovarianceMatrix(4, 4)), locNeutralShower->dDetectorSystem);
			locNeutralParticleHypothesis->setPathLength(locPathLength, 0.0); //zero uncertainty (for now)
			locNeutralParticleHypothesis->setErrorMatrix(locParticleCovariance);

			locNeutralParticleHypothesis->setPID(locPIDHypotheses[loc_k]);
			locNeutralParticleHypothesis->dChiSq = locChiSq;
			locNeutralParticleHypothesis->dNDF = locNDF;
			locNeutralParticleHypothesis->dFOM = locFOM;

			_data.push_back(locNeutralParticleHypothesis);	
		} //end PID loop
	} //end DNeutralShower loop

	return NOERROR;
}

//------------------
// erun
//------------------
jerror_t DNeutralParticleHypothesis_factory::erun(void)
{
	return NOERROR;
}

//------------------
// fini
//------------------
jerror_t DNeutralParticleHypothesis_factory::fini(void)
{
	return NOERROR;
}

void DNeutralParticleHypothesis_factory::Calc_ParticleCovariance(const DNeutralShower* locNeutralShower, double locMass, const DVector3& locMomentum, const DVector3& locPathVector, DMatrixDSym& locParticleCovariance)
{
	//build 8x8 matrix: 5x5 shower, 3x3 vertex position
	DMatrixDSym locShowerPlusVertCovariance(8);
	for(unsigned int loc_l = 0; loc_l < 5; ++loc_l) //shower: e, x, y, z, t
	{
		for(unsigned int loc_m = 0; loc_m < 5; ++loc_m)
			locShowerPlusVertCovariance(loc_l, loc_m) = locNeutralShower->dCovarianceMatrix(loc_l, loc_m);
	}
	locShowerPlusVertCovariance(5, 5) = 0.25*dTargetRadius*dTargetRadius/12.0; //vertex position x
	locShowerPlusVertCovariance(6, 6) = 0.25*dTargetRadius*dTargetRadius/12.0; //vertex position y
	locShowerPlusVertCovariance(7, 7) = dTargetLength*dTargetLength/12.0; //vertex position z

	DVector3 locDeltaX = -1.0*locPathVector; //defined oppositely in document!
	DVector3 locDeltaXOverDeltaXSq = (1.0/locDeltaX.Mag2())*locDeltaX;
	DVector3 locEPVecOverPSq = (locNeutralShower->dEnergy/locMomentum.Mag2())*locMomentum;
	DVector3 locEPVecOverCPMagDeltaXMag = (locNeutralShower->dEnergy/(SPEED_OF_LIGHT*locDeltaX.Mag()*locMomentum.Mag()))*locDeltaX;

	//build transform matrix
	DMatrix locTransformMatrix(7, 8);

	locTransformMatrix(0, 0) = locEPVecOverPSq.X(); //partial deriv of px wrst shower-e
	locTransformMatrix(0, 1) = locMomentum.Px()*(locDeltaXOverDeltaXSq.X() - 1.0/locDeltaX.X()); //partial deriv of px wrst shower-x
	locTransformMatrix(0, 2) = locMomentum.Px()*locDeltaX.Y()/locDeltaX.Mag2(); //partial deriv of px wrst shower-y
	locTransformMatrix(0, 3) = locMomentum.Px()*locDeltaX.Z()/locDeltaX.Mag2(); //partial deriv of px wrst shower-z
	locTransformMatrix(0, 5) = -1.0*locTransformMatrix(0, 1); //partial deriv of px wrst vert-x
	locTransformMatrix(0, 6) = -1.0*locTransformMatrix(0, 2); //partial deriv of px wrst vert-y
	locTransformMatrix(0, 7) = -1.0*locTransformMatrix(0, 3); //partial deriv of px wrst vert-z

	locTransformMatrix(1, 0) = locEPVecOverPSq.Y(); //partial deriv of py wrst shower-e
	locTransformMatrix(1, 1) = locMomentum.Py()*locDeltaX.X()/locDeltaX.Mag2(); //partial deriv of py wrst shower-x
	locTransformMatrix(1, 2) = locMomentum.Py()*(locDeltaXOverDeltaXSq.Y() - 1.0/locDeltaX.Y()); //partial deriv of py wrst shower-y
	locTransformMatrix(1, 3) = locMomentum.Py()*locDeltaX.Z()/locDeltaX.Mag2(); //partial deriv of py wrst shower-z
	locTransformMatrix(1, 5) = -1.0*locTransformMatrix(1, 1); //partial deriv of py wrst vert-x
	locTransformMatrix(1, 6) = -1.0*locTransformMatrix(1, 2); //partial deriv of py wrst vert-y
	locTransformMatrix(1, 7) = -1.0*locTransformMatrix(1, 3); //partial deriv of py wrst vert-z

	locTransformMatrix(2, 0) = locEPVecOverPSq.Z(); //partial deriv of pz wrst shower-e
	locTransformMatrix(2, 1) = locMomentum.Pz()*locDeltaX.X()/locDeltaX.Mag2(); //partial deriv of pz wrst shower-x
	locTransformMatrix(2, 2) = locMomentum.Pz()*locDeltaX.Y()/locDeltaX.Mag2(); //partial deriv of pz wrst shower-y
	locTransformMatrix(2, 3) = locMomentum.Pz()*(locDeltaXOverDeltaXSq.Z() - 1.0/locDeltaX.Z()); //partial deriv of pz wrst shower-z
	locTransformMatrix(2, 5) = -1.0*locTransformMatrix(2, 1); //partial deriv of pz wrst vert-x
	locTransformMatrix(2, 6) = -1.0*locTransformMatrix(2, 2); //partial deriv of pz wrst vert-y
	locTransformMatrix(2, 7) = -1.0*locTransformMatrix(2, 3); //partial deriv of pz wrst vert-z

	locTransformMatrix(3, 5) = 1.0; //partial deriv of x wrst vertex-x
	locTransformMatrix(4, 6) = 1.0; //partial deriv of y wrst vertex-y
	locTransformMatrix(5, 7) = 1.0; //partial deriv of z wrst vertex-z

	locTransformMatrix(6, 0) = locMass*locMass*locDeltaX.Mag()/(SPEED_OF_LIGHT*locMomentum.Mag()*locMomentum.Mag2()); //partial deriv of t wrst shower-e
	locTransformMatrix(6, 1) = locEPVecOverCPMagDeltaXMag.X(); //partial deriv of t wrst shower-x
	locTransformMatrix(6, 2) = locEPVecOverCPMagDeltaXMag.Y(); //partial deriv of t wrst shower-y
	locTransformMatrix(6, 3) = locEPVecOverCPMagDeltaXMag.Z(); //partial deriv of t wrst shower-z
	locTransformMatrix(6, 4) = 1.0; //partial deriv of t wrst shower-t
	locTransformMatrix(6, 5) = -1.0*locTransformMatrix(6, 1); //partial deriv of t wrst vert-x
	locTransformMatrix(6, 6) = -1.0*locTransformMatrix(6, 2); //partial deriv of t wrst vert-y
	locTransformMatrix(6, 7) = -1.0*locTransformMatrix(6, 3); //partial deriv of t wrst vert-z

	//convert
	locParticleCovariance.ResizeTo(7, 7);
	locParticleCovariance = locShowerPlusVertCovariance.Similarity(locTransformMatrix);
}

