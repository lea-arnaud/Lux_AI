#pragma once
#include <vector>
#include <string>
#include <stdio.h>
#include <fstream>
#include <chrono>
#include <ctime>
#include <algorithm>
#include "BehaviorTree.h"
#include "BehaviorTreeNames.h"

using namespace std;

namespace statistics {


	struct TurnStats {
		unsigned int p1cities;
		unsigned int p2cities;
		unsigned int turnNb;
		unsigned int p1resources;
		unsigned int p2resources;
		unsigned int p1agents;
		unsigned int p2agents;
	public:

		TurnStats(Blackboard& blackboard )
		{
			turnNb = blackboard.getData<size_t>( bbn::GLOBAL_TURN);
			p1cities = blackboard.getData<size_t> ( bbn::GLOBAL_FRIENDLY_CITY_COUNT );
			p2cities = blackboard.getData<size_t> ( bbn::GLOBAL_CITY_COUNT ) - p1cities;
			//turnNb = blackboard.getData<size_t> ( bbn::GLOBAL_TURN );
			//turnNb = blackboard.getData<size_t> ( bbn::GLOBAL_TURN );
		}

		friend ostream& operator<<( ostream& os, const TurnStats& ts ) {
			return os << "Turn " << ts.turnNb << " : " << "P1 => " << ts.p1cities << "; P2 => " << ts.p2cities;
		}
	};

	class GameStats {
	private:
		string fileName;
		ofstream file;
	public:
		GameStats() : file{ }
		{
			auto end = chrono::system_clock::now();
			auto endtxt = chrono::system_clock::to_time_t(end);// + string(ctime(&endtxt)));
			string timetxt = to_string(endtxt);

			fileName = "./../../results/" + params::statPath;
		}
;

		void printGameStats (shared_ptr<Blackboard> blackboard) {

			file.open("./../../results/" + params::statPath + ".txt", ios::in |ios::out|ios::app);

			size_t turnNb = blackboard->getData<size_t>(bbn::GLOBAL_TURN);
			int p1cities = blackboard->getData<int>(bbn::GLOBAL_FRIENDLY_CITY_COUNT);
			int p2cities = blackboard->getData<int>(bbn::GLOBAL_CITY_COUNT) - p1cities;

			file << "Turn " << turnNb << " : " << "P1 => &" << p1cities << "&; P2 => &" << p2cities << "&\n";

			file.close();
		}
	};

	static GameStats gameStats{};
}