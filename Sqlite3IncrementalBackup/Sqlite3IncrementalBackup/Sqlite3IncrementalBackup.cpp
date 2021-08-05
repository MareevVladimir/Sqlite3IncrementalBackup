// Sqlite3IncrementalBackup.cpp : Определяет функции для статической библиотеки.
//

#include "api.h"

#include <exception>
#include <string.h>

#include "backup.h"

namespace sqlite3_inc_bkp {
	
	bool backup(sqlite3* db, const char* path, const char* name, char **errmsg, std::function<uint64_t(const void*, std::size_t)> f)
	{
		try {
			IBackup::Create(IBackup::Version::V1, path, name, f)->DoBackup(db);
		}
		catch (const BackupException &e) {
			if (errmsg) {				
				const std::size_t msgSize = strlen(e.what());
				*errmsg = new char[msgSize + 1];
				strcpy_s(*errmsg, msgSize + 1, e.what());
			}
			return false;
		}
		catch (const std::exception &e) {
			if (errmsg) {
				const std::size_t msgSize = strlen(e.what());
				*errmsg = new char[msgSize + 1];
				strcpy_s(*errmsg, msgSize + 1, e.what());
			}
			return false;
		}
	}
}
