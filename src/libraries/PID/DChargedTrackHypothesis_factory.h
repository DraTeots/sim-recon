// $Id$
//
//    File: DChargedTrackHypothesis_factory.h
// Created: Tue Aug  9 14:29:24 EST 2011
// Creator: pmatt (on Linux ifarml6 2.6.18-128.el5 x86_64)
//

#ifndef _DChargedTrackHypothesis_factory_
#define _DChargedTrackHypothesis_factory_

#include <iostream>
#include <iomanip>

#include <JANA/JFactory.h>
#include <PID/DChargedTrackHypothesis.h>
#include <PID/DDetectorMatches.h>
#include <TRACKING/DTrackTimeBased.h>
#include <PID/DParticleID.h>
#include <TOF/DTOFPoint.h>
#include <BCAL/DBCALShower.h>
#include <FCAL/DFCALShower.h>
#include "DResourcePool.h"

using namespace std;
using namespace jana;

class DChargedTrackHypothesis_factory:public jana::JFactory<DChargedTrackHypothesis>
{
	public:
		DChargedTrackHypothesis_factory(){};
		~DChargedTrackHypothesis_factory(){};

		DChargedTrackHypothesis* Create_ChargedTrackHypothesis(JEventLoop* locEventLoop, const DTrackTimeBased* locTrackTimeBased, const DDetectorMatches* locDetectorMatches, const DEventRFBunch* locEventRFBunch) const;
		void Add_TimeToTrackingMatrix(DChargedTrackHypothesis* locChargedTrackHypothesis, TMatrixFSym* locCovarianceMatrix, double locFlightTimeVariance, double locHitTimeVariance, double locFlightTimePCorrelation) const;

		void Recycle_Hypotheses(vector<const DChargedTrackHypothesis*>& locHypos){dResourcePool_ChargedTrackHypothesis.Recycle(locHypos);}
		void Recycle_Hypotheses(vector<DChargedTrackHypothesis*>& locHypos){dResourcePool_ChargedTrackHypothesis.Recycle(locHypos);}
		void Recycle_Hypothesis(const DChargedTrackHypothesis* locHypo){dResourcePool_ChargedTrackHypothesis.Recycle(locHypo);}

		DChargedTrackHypothesis* Get_Resource(void)
		{
			auto locHypo = dResourcePool_ChargedTrackHypothesis.Get_Resource();
			locHypo->Reset();
			return locHypo;
		}

	private:
		const DParticleID* dPIDAlgorithm;

		//RESOURCE POOL
		DResourcePool<DChargedTrackHypothesis> dResourcePool_ChargedTrackHypothesis;

		jerror_t init(void);						///< Called once at program start.
		jerror_t brun(jana::JEventLoop *locEventLoop, int32_t runnumber);	///< Called everytime a new run number is detected.
		jerror_t evnt(jana::JEventLoop *locEventLoop, uint64_t eventnumber);	///< Called every event.
};

#endif // _DChargedTrackHypothesis_factory_

