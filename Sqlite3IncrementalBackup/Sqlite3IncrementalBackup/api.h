#ifndef API_H
#define API_H

// Добавьте сюда заголовочные файлы для предварительной компиляции
#define SQLITE_ENABLE_DBPAGE_VTAB 1
#include "sqlite3.h"
#include <functional>

namespace sqlite3_inc_bkp {	
	bool backup(sqlite3* db, const char* path, const char* name, char** errmsg, std::function<uint64_t(const void*, std::size_t)> f);
}

#endif //API_H
