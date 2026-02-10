// engine/systems/ai/ai_service.hpp
#pragma once
#include <string>

namespace epoch::ai {

	void init_bot();
	void shutdown_bot();
	std::string send_to_bot(const std::string& user_text);

}
