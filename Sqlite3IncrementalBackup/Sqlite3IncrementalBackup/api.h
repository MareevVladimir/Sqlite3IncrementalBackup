#ifndef API_H
#define API_H

#include <functional>

struct sqlite3;
namespace sqlite3_inc_bkp {	
	/// <summary>
	/// API method to make an incremental backup of your open SQLITE3 database
	/// </summary>
	/// <param name="db">Opened SQLITE3 database instance</param>
	/// <param name="path">Path to backup directory</param>
	/// <param name="name">Name of backup</param>
	/// <param name="errmsg">Pointer to write error message, if returned true nothing will be written</param>
	/// <param name="f">callback of hash algorithm uint64_t<hash sum> hash_algorithm(const void *<data>, size_t<size of data>)</param>
	/// <returns>true if backup success</returns>
	bool backup(sqlite3* db, const char* path, const char* name, char** errmsg, std::function<uint64_t(const void*, std::size_t)> f);
}

#endif //API_H
