// $Id$
//
//    File: Df250TriggerTime.h
// Created: Tue Aug  7 15:24:10 EDT 2012
// Creator: davidl (on Darwin harriet.jlab.org 11.4.0 i386)
//

#ifndef _Df250TriggerTime_
#define _Df250TriggerTime_

#include <JANA/jerror.h>
#include <JANA/JFactory.h>

class Df250TriggerTime:public jana::JObject{
	
	/// Holds trigger time data for one event in
	/// a single f250 Flash ADC module.
	
	public:
		JOBJECT_PUBLIC(Df250TriggerTime);

		Df250TriggerTime():rocid(0),slot(0),itrigger(0),time(0){}
		Df250TriggerTime(uint32_t rocid, uint32_t slot, uint32_t itrigger, uint64_t time):rocid(rocid),slot(slot),itrigger(itrigger),time(time){}
		
		uint32_t rocid;          // from EVIO header (crate number)
		uint32_t slot;           // from Block Header
		uint32_t itrigger;       // from Event Header
		uint64_t time;           // from Trigger Time words
		
	
		// This method is used primarily for pretty printing
		// the second argument to AddString is printf style format
		void toStrings(vector<pair<string,string> > &items)const{
			AddString(items, "rocid", "%d", rocid);
			AddString(items, "slot", "%d", slot);
			AddString(items, "itrigger", "%d", itrigger);
			AddString(items, "time", "%ld", time);
		}

};

#endif // _Df250TriggerTime_

