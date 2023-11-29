#pragma once

#include <string>
#include <fstream>

#include "BehaviorTree.h"
#include "BehaviorTreeNames.h"

namespace statistics {

	struct TurnStats {
		unsigned int p1cities;
		unsigned int p2cities;
		unsigned int turnNb;
		unsigned int p1resources{};
		unsigned int p2resources{};
		unsigned int p1agents{};
		unsigned int p2agents{};

		explicit TurnStats(Blackboard &blackboard)
		{
			turnNb   = static_cast<unsigned int>(blackboard.getData<size_t>(bbn::GLOBAL_TURN));
			p1cities = static_cast<unsigned int>(blackboard.getData<int>(bbn::GLOBAL_FRIENDLY_CITY_COUNT));
			p2cities = static_cast<unsigned int>(blackboard.getData<int>(bbn::GLOBAL_CITY_COUNT) - p1cities);
		}

		friend std::ostream& operator<<(std::ostream& os, const TurnStats& ts ) {
			return os << "Turn " << ts.turnNb << " : " << "P1 => &" << ts.p1cities << "&; P2 => &" << ts.p2cities << "&";
		}
	};

	class GameStats {
	public:
		void printGameStats(const std::shared_ptr<Blackboard> &blackboard) const
		{
			std::ofstream file{ "..\\..\\results\\" + params::statPath + ".txt", std::ios::app };
			file << TurnStats{ *blackboard } << '\n';
		}
	};

	extern GameStats gameStats;
}