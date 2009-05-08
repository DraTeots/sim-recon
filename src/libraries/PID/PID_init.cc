// $Id: PID_init.cc 2433 2007-04-07 14:57:32Z kornicer $

#include <JANA/JEventLoop.h>
using namespace jana;

#include "DPhoton_factory.h"
#include "DTwoGammaFit_factory.h"
#include "DTwoGammaFit_factory_PI0.h"
#include "DTwoGammaFit_factory_ETA.h"
#include "DParticle_factory.h"
#include "DParticle_factory_THROWN.h"

#include "DBeamPhoton.h"
typedef JFactory<DBeamPhoton> DBeamPhoton_factory;

#define UC_CLUSTERIZER

jerror_t PID_init(JEventLoop *loop)
{
	/// Create and register PID data factories
	loop->AddFactory(new DPhoton_factory());
//	loop->AddFactory(new DPi0_factory());
//	loop->AddFactory(new DTwoGammaFit_factory(0.135));
	loop->AddFactory(new DTwoGammaFit_factory_PI0);
	loop->AddFactory(new DTwoGammaFit_factory_ETA);
	loop->AddFactory(new DBeamPhoton_factory);
	loop->AddFactory(new DParticle_factory);
	loop->AddFactory(new DParticle_factory_THROWN);

	return NOERROR;
}
