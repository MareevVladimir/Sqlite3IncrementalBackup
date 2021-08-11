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
	/// <returns>0 if success, -1 if unhandled exception, > 0 error code </returns>	
	int backup(sqlite3* db, const char* path, const char* name, char** errmsg, std::function<uint64_t(const void*, std::size_t)> f);

	/// <summary>
	/// API method to read an incremental backup to your open SQLITE3 database
	/// </summary>
	/// <param name="dst">Opened SQLITE3 database instance</param>
	/// <param name="path">Path to backup directory</param>
	/// <param name="name">Name of backup</param>
	/// <param name="errmsg">Pointer to write error message, if returned true nothing will be written</param>
	/// <param name="f">callback of hash algorithm uint64_t<hash sum> hash_algorithm(const void *<data>, size_t<size of data>)</param>
	/// <returns>0 if success, -1 if unhandled exception, > 0 error code </returns>	
	int read_backup(sqlite3* dst, const char* path, const char* name, char** errmsg, std::function<uint64_t(const void*, std::size_t)> f);

	/// <summary>
	/// API method to clear an incremental backup
	/// </summary>
	/// <param name="path">Path to backup directory</param>
	/// <param name="name">Name of backup</param>
	/// <param name="errmsg">Pointer to write error message, if returned true nothing will be written</param>
	/// <returns>0 if success, -1 if unhandled exception, > 0 error code </returns>	
	int clear_backup(const char* path, const char* name, char** errmsg);
}

#endif //API_H
