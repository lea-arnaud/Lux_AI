#pragma once

#include <string>
#include <fstream>
#include <chrono>

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
			return os << "Turn " << ts.turnNb << " : " << "P1 => " << ts.p1cities << "; P2 => " << ts.p2cities;
		}
	};

	class GameStats {
	private:
		std::string m_fileName;
	public:
		GameStats()
		{
			auto end = std::chrono::system_clock::now();
			auto endtxt = std::chrono::system_clock::to_time_t(end);
			std::string timetxt = std::to_string(endtxt);

			m_fileName = "..\\..\\" + params::statPath;
			std::ofstream{ m_fileName }; // create or clear the file
		}

		void printGameStats(const std::shared_ptr<Blackboard> &blackboard) const
		{
			std::ofstream file{ m_fileName, std::ios::app };
			file << "Turn " << TurnStats{ *blackboard } << '\n';
		}

	};

	extern GameStats gameStats;
}