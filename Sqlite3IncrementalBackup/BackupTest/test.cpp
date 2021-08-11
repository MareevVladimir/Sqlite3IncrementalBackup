#include "pch.h"
#include "sqlite3.h"
#include <iostream>
#include <xxhash.h>
#include <fstream>
#include <chrono>
#include <boost/filesystem.hpp>
#include <Sqlite3IncrementalBackup/api.h>

#define TIMER_START(timer_name) auto _##timer_name = std::chrono::high_resolution_clock::now();
#define TIMER_GET(timer_name, measure) std::chrono::duration_cast<std::chrono::##measure>(std::chrono::high_resolution_clock::now() - _##timer_name).count()

#define IN_MEMORY_CACHE

#ifdef IN_MEMORY_CACHE    
	static const int g_flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_MEMORY;
#else    
	static const int g_flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_SHAREDCACHE | SQLITE_OPEN_NOMUTEX;
#endif

#ifdef IN_MEMORY_CACHE
	static const char* dbPath = "test";
#else
	static const char* dbPath = ".\\test.sqlite";
#endif
static const char* dbBackupPath = ".\\test_backup.sqlite";

static const int loadParameter = 1000;

sqlite3* openDb() {
	sqlite3* db = nullptr;
#ifdef IN_MEMORY_CACHE
	if (boost::filesystem::exists(dbBackupPath)) {
		sqlite3* bckp = nullptr;
		sqlite3_open_v2(dbBackupPath, &bckp, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_SHAREDCACHE | SQLITE_OPEN_NOMUTEX, nullptr);
		sqlite3_open_v2(dbPath, &db, g_flags, nullptr);
		auto loadFromBackup = sqlite3_backup_init(
			db, "main",
			bckp, "main");
		
		sqlite3_backup_step(loadFromBackup, -1);		
		sqlite3_backup_finish(loadFromBackup);
	}
	else
#endif
	sqlite3_open_v2(dbPath, &db, g_flags, nullptr);	
	return db;
}

void backupNative(sqlite3* db) {
	sqlite3* bck = nullptr;
	sqlite3_open_v2("testnative.sqlite", &bck, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_SHAREDCACHE | SQLITE_OPEN_NOMUTEX, nullptr);
	sqlite3_backup* pBckHandle = sqlite3_backup_init(bck, "main", db, "main");	
	sqlite3_backup_step(pBckHandle, -1);
	sqlite3_backup_finish(pBckHandle);
}

sqlite3* createDb() {
	sqlite3* db = openDb();
	int rc = sqlite3_exec(db, "DROP TABLE IF EXISTS test", NULL, NULL, NULL);
	rc |= sqlite3_exec(db, "CREATE TABLE test (col1 NUM PRIMARY KEY, col2 TEXT)", NULL, NULL, NULL);
	if (rc != SQLITE_OK)
		std::cerr << "error query\n";
	return db;
}

void fillDb(sqlite3* db, int rowCount, std::function<std::string()> f) {

	sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, NULL);
	sqlite3_stmt* stmt = nullptr;
	if (SQLITE_OK != sqlite3_prepare_v2(db, "INSERT INTO test (col1, col2) VALUES (@col1, @col2)", -1, &stmt, NULL)) {
		std::cerr << "error query\n";
	}
	
	for (int i = 0; i < rowCount; ++i) {
		std::string str = f();
		sqlite3_bind_int64(stmt, sqlite3_bind_parameter_index(stmt, "@col1"), i + 1);
		sqlite3_bind_text(stmt, sqlite3_bind_parameter_index(stmt, "@col2"), str.c_str(), str.size(), nullptr);
		sqlite3_step(stmt);
		sqlite3_clear_bindings(stmt);
		sqlite3_reset(stmt);
	}
	sqlite3_finalize(stmt);
	sqlite3_exec(db, "END TRANSACTION", NULL, NULL, NULL);
}

void updateDb(sqlite3* db, int from, int rowCount, std::function<std::string()> f) {
	sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, NULL);
	sqlite3_stmt* stmt = nullptr;
	sqlite3_prepare_v2(db, "UPDATE test SET col2=@col2 WHERE col1=@col1", -1, &stmt, NULL);
	
	for (int i = from; i < rowCount + from; ++i) {
		std::string str = f();
		sqlite3_bind_text(stmt, sqlite3_bind_parameter_index(stmt, "@col2"), str.c_str(), str.size(), nullptr);
		sqlite3_bind_int64(stmt, sqlite3_bind_parameter_index(stmt, "@col1"), i + 1);
		sqlite3_step(stmt);
		sqlite3_clear_bindings(stmt);
		sqlite3_reset(stmt);
	}
	sqlite3_finalize(stmt);
	sqlite3_exec(db, "END TRANSACTION", NULL, NULL, NULL);
}

std::string genWord() {
	switch (rand() % 5) {
	case 0: return "it";
	case 1: return "is";
	case 2: return "wednesday";
	case 3: return "my";
	case 4: return "dudes";
	}
}

bool compareDb() {
#ifdef IN_MEMORY_CACHE
	return true;
#endif 

	std::ifstream fsrc(dbPath, std::ios::binary);
	std::ifstream f1(dbPath, std::ifstream::binary | std::ifstream::ate);
	std::ifstream f2(dbBackupPath, std::ifstream::binary | std::ifstream::ate);

	if (f1.fail() || f2.fail()) {
		std::cerr << "File problem";
		return false; 
	}

	if (f1.tellg() != f2.tellg()) {
		std::cerr << "Size mismatch";
		return false; 
	}

	//seek back to beginning and use std::equal to compare contents
	f1.seekg(0, std::ifstream::beg);
	f2.seekg(0, std::ifstream::beg);
	return std::equal(std::istreambuf_iterator<char>(f1.rdbuf()),
		std::istreambuf_iterator<char>(),
		std::istreambuf_iterator<char>(f2.rdbuf()));
}

bool backup(sqlite3 *db) {
	char* msg = nullptr;
	bool status = 0 == sqlite3_inc_bkp::backup(db, ".\\", "test", &msg, [](const void* data, std::size_t size) { return XXH64(data, size, 0); });
	/*if (!status)
		std::cerr << msg << std::endl;*/
	return status;
}

void clear() {
	std::remove(".\\.test.manifest");
	std::remove(dbPath);
	std::remove(dbBackupPath);
	std::remove(".\\testnative.sqlite");
}

sqlite3* openBackup() {
	sqlite3* db = nullptr;
	if (boost::filesystem::exists(dbBackupPath)) {		
		sqlite3_open_v2(dbBackupPath, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_SHAREDCACHE | SQLITE_OPEN_NOMUTEX, nullptr);		
	}	
	return db;
}

TEST(InitBackup, BackupTest) {
	clear();

	sqlite3* db = createDb();	
	fillDb(db, 100 * loadParameter, []{ return std::string("test"); });
	std::cout << "Backup started...\n";
	TIMER_START(backupTimer);
	EXPECT_TRUE(backup(db));
	auto time = TIMER_GET(backupTimer, milliseconds);
	TIMER_START(nativeTimer);
	backupNative(db);
	auto timeNative = TIMER_GET(nativeTimer, milliseconds);
	std::cout << "Backup time [" << time << "]ms" << " VS Native backup time["<< timeNative <<"]ms"<<std::endl;

	EXPECT_TRUE(compareDb());
}


TEST(UpdateAndBackup, BackupTest) {
	sqlite3* db = openDb();
	updateDb(db, 0, 100 * loadParameter, genWord);
	std::cout << "Backup started...\n";
	TIMER_START(backupTimer);
	EXPECT_TRUE(backup(db));
	auto time = TIMER_GET(backupTimer, milliseconds);
	TIMER_START(nativeTimer);
	backupNative(db);
	auto timeNative = TIMER_GET(nativeTimer, milliseconds);
	std::cout << "Backup time [" << time << "]ms" << " VS Native backup time[" << timeNative << "]ms" << std::endl;
	EXPECT_TRUE(compareDb());	
}

TEST(WriteMoreAndBackup, BackupTest) {
	sqlite3* db = openDb();
	fillDb(db, 4000 * loadParameter, [] { return std::string("test"); });	
	std::cout << "Backup started...\n";
	TIMER_START(backupTimer);
	EXPECT_TRUE(backup(db));
	auto time = TIMER_GET(backupTimer, milliseconds);
	TIMER_START(nativeTimer);
	backupNative(db);
	auto timeNative = TIMER_GET(nativeTimer, milliseconds);
	std::cout << "Backup time [" << time << "]ms" << " VS Native backup time[" << timeNative << "]ms" << std::endl;
	EXPECT_TRUE(compareDb());
}

TEST(UpdatePartAndBackup, BackupTest) {
	sqlite3* db = openDb();
	updateDb(db, 200 * loadParameter, loadParameter, genWord);
	std::cout << "Backup started...\n";
	TIMER_START(backupTimer);
	EXPECT_TRUE(backup(db));
	auto time = TIMER_GET(backupTimer, milliseconds);
	TIMER_START(nativeTimer);
	backupNative(db);
	auto timeNative = TIMER_GET(nativeTimer, milliseconds);
	std::cout << "Backup time [" << time << "]ms" << " VS Native backup time[" << timeNative << "]ms" << std::endl;
	EXPECT_TRUE(compareDb());
}

TEST(CorruptCheck, BackupTest) {
	//Check integrity true
	char* msg;
	sqlite3* bckp = openBackup();
	sqlite3* dst = nullptr;	
	sqlite3_open_v2(dbPath, &dst, g_flags, nullptr);
	int rc = sqlite3_inc_bkp::read_backup(dst, ".\\", "test", &msg, [](const void* data, std::size_t size) { return XXH64(data, size, 0); });
	if (rc != 0) {
		std::cout << "Corrupt message: " << msg << std::endl;
	}
	sqlite3_close_v2(dst);
	EXPECT_TRUE(rc == 0);
	
	//Corrupt manifest file	
	boost::filesystem::copy(".\\.test.manifest", ".\\test.manifest.bcp");
	std::ofstream file(".\\.test.manifest", std::ios::binary);
	file.write("it is wednesday my dudes", 333);
	file.close();
	rc = sqlite3_inc_bkp::read_backup(bckp, ".\\", "test", &msg, [](const void* data, std::size_t size) { return XXH64(data, size, 0); });
	EXPECT_TRUE(rc == 5);
	std::remove(".\\.test.manifest");
	std::rename(".\\test.manifest.bcp", ".\\.test.manifest");

	//Corrupt database file
	updateDb(bckp, 200 * loadParameter, loadParameter, genWord);
	sqlite3* db = nullptr;
	rc = sqlite3_inc_bkp::read_backup(bckp, ".\\", "test", &msg, [](const void* data, std::size_t size) { return XXH64(data, size, 0); });	
	EXPECT_TRUE(rc == 5);
	clear();
}
