// Sqlite3IncrementalBackup.cpp : Определяет функции для статической библиотеки.
//

#include "api.h"

#include <exception>
#include <string.h>

#include "backup.h"

namespace sqlite3_inc_bkp {
	
	void handleError(const std::exception& e, char **errmsg) {
		if (errmsg) {
			const std::size_t msgSize = strlen(e.what());
			*errmsg = new char[msgSize + 1];
			strcpy_s(*errmsg, msgSize + 1, e.what());
		}
	}

	int backup(sqlite3* db, const char* path, const char* name, char **errmsg, std::function<uint64_t(const void*, std::size_t)> f)
	{
		try {
			IBackup::Create(IBackup::Version::V1, path, name, f)->Write(db);
			return 0;
		}
		catch (const BackupException &e) {
			handleError(e, errmsg);
			return static_cast<int>(e.code());
		}
		catch (const std::exception &e) {
			handleError(e, errmsg);
			return -1;
		}
	}

	int read_backup(sqlite3* dst, const char* path, const char* name, char** errmsg, std::function<uint64_t(const void*, std::size_t)> f) {
		try {
			IBackup::Create(IBackup::Version::V1, path, name, f)->Read(dst);
			return 0;
		}
		catch (const BackupException& e) {
			handleError(e, errmsg);
			return static_cast<int>(e.code());
		}
		catch (const std::exception& e) {
			handleError(e, errmsg);
			return -1;
		}
	}

	int clear_backup(const char* path, const char* name, char** errmsg) {
		try {
			IBackup::Create(IBackup::Version::V1, path, name, nullptr)->Clear();
			return 0;
		}
		catch (const BackupException& e) {
			handleError(e, errmsg);
			return static_cast<int>(e.code());
		}
		catch (const std::exception& e) {
			handleError(e, errmsg);
			return -1;
		}
	}
}
