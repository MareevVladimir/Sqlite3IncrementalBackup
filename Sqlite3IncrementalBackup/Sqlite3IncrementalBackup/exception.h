#pragma once

#include <exception>
#include "common.h"

namespace sqlite3_inc_bkp {
	class BackupException : public std::exception {
	public:
		enum class Error : std::size_t {
			Unknown,
			SelectPages,
			BackupInit
		};
		inline Error code() const { return _error; }
	private:
		static constexpr const char* ErrorStr(Error code) {
			switch (code) {
			case Error::SelectPages:
				return "Failed get database page data";
			case Error::BackupInit:
				return "Failed init backup file";
			case Error::Unknown:
			default:
				return "Unknown error";
			}
		}

		std::string ErrorFullStr(Error code, const char* message) {
			char msg[MESSAGE_BUFFER_SIZE];
			if (!strcmp(message, ""))
				std::snprintf(msg, MESSAGE_BUFFER_SIZE, "(%d) %s", static_cast<std::size_t>(code), ErrorStr(code));
			else
				std::snprintf(msg, MESSAGE_BUFFER_SIZE, "(%d) %s: %s", static_cast<std::size_t>(code), ErrorStr(code), message);

			return std::string(msg);
		}
	public:
		BackupException(const char* message, Error err = Error::Unknown) : _error(err), std::exception(ErrorFullStr(err, message).c_str()) {}

	private:
		Error _error;
	};
}