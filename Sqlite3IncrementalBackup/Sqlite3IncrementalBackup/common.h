#pragma once

#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>

#define MESSAGE_BUFFER_SIZE 0x100

namespace sqlite3_inc_bkp {
	namespace tools {
		struct FormatString {
		public:
			template <typename... Args>
			static std::string format(
				std::string formatString,
				Args&&... args)
			{
				boost::format f(std::move(formatString));
				return formatImpl(f, std::forward<Args>(args)...).str();
			}
		private:
			static boost::format& formatImpl(boost::format& f)
			{
				return f;
			}

			template <typename Head, typename... Tail>
			static boost::format& formatImpl(
				boost::format& f,
				Head const& head,
				Tail&&... tail)
			{
				return formatImpl(f % head, std::forward<Tail>(tail)...);
			}
		};
	}
}
